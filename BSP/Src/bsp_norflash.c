
#include "stm32f1xx_hal.h"
#include "spi.h"
#include "bsp_norflash.h"

#define CS_GPIO (GPIOB)
#define CS_Pin  (GPIO_PIN_0)
#define CS_High (CS_GPIO->BSRR = CS_Pin)
#define CS_Low  (CS_GPIO->BSRR = CS_Pin << 16)

#define NOR_PAGE_SIZE   0x100
#define NOR_SECTOR_SIZE 0x1000
#define AND_NOR_SECTOR  0xFFFFF000

static char nor_buf[NOR_SECTOR_SIZE] = {0};

static void wait_write_enable(void)
{
	uint8_t status = 0;

	CS_Low;
	HAL_SPI_Transmit(&hspi1, (uint8_t []){0x05}, 1, HAL_MAX_DELAY);
	do
		HAL_SPI_Receive(&hspi1, &status, 1, HAL_MAX_DELAY);
	while (!(status & 0x02));
	CS_High;
}

static void write_enable(void)
{
	CS_Low;
	HAL_SPI_Transmit(&hspi1, (uint8_t []){0x06}, 1, HAL_MAX_DELAY);
	CS_High;
}

static void wait_busy(void)
{
	uint8_t status = 0;

	CS_Low;
	HAL_SPI_Transmit(&hspi1, (uint8_t []){0x05}, 1, HAL_MAX_DELAY);
	do
		HAL_SPI_Receive(&hspi1, &status, 1, HAL_MAX_DELAY);
	while (status & 0x01);
	CS_High;
}

static void data_read(int addr, void *buf, int length)
{
	addr <<= 8;
	addr = __REV(addr);

	wait_busy();

	CS_Low;
	HAL_SPI_Transmit(&hspi1, (uint8_t []){0x03}, 1, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi1, (uint8_t *)&addr, 3, HAL_MAX_DELAY);
	HAL_SPI_Receive(&hspi1, buf, length, HAL_MAX_DELAY);
	CS_High;
}

static int check_blank(int addr, int length)
{
	data_read(addr, nor_buf, length);

	for (int i = 0; i < length; i++)
		if (nor_buf[i] != 0xFF)
			return -1;

	return 0;
}

static void sector_erase(int addr)
{
	addr <<= 8;
	addr = __REV(addr);

	write_enable();
	wait_write_enable();

	CS_Low;
	HAL_SPI_Transmit(&hspi1, (uint8_t []){0x20}, 1, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi1, (uint8_t *)&addr, 3, HAL_MAX_DELAY);
	CS_High;

    wait_busy();
}

static void page_program(int addr, void *buf, int length)
{
	addr <<= 8;
	addr = __REV(addr);

	write_enable();
	wait_write_enable();

	CS_Low;
	HAL_SPI_Transmit(&hspi1, (uint8_t []){0x02}, 1, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi1, (uint8_t *)&addr, 3, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi1, buf, length, HAL_MAX_DELAY);
	CS_High;

    wait_busy();
}

static void data_write(int addr, void *buf, int length)
{
	int  Length = 0;
	int  inPageLength = 0;
	char *pData = NULL;
	char *wData = buf;

	while(length > 0)
	{
		if((addr % NOR_SECTOR_SIZE) + length > NOR_SECTOR_SIZE)
			Length = NOR_SECTOR_SIZE - (addr % NOR_SECTOR_SIZE);
		else
			Length = length;

		//判断写入位置是否为空
		if(check_blank(addr, Length) == 0)
		{
			//为空时，直接写入到NorFlash
			while(Length > 0)
			{
				//写入数据截断在Page内
				if((addr % NOR_PAGE_SIZE) + Length > NOR_PAGE_SIZE)
					inPageLength = NOR_PAGE_SIZE - (addr % NOR_PAGE_SIZE);
				else
					inPageLength = Length;

				page_program(addr, wData, inPageLength);

				addr   += inPageLength;
				wData  += inPageLength;
				Length -= inPageLength;
				length -= inPageLength;
			}
		}
		else
		{
			//为非空时，将数据拷贝至Buffer对应位置
			data_read(addr & AND_NOR_SECTOR, nor_buf, NOR_SECTOR_SIZE);

			pData = nor_buf + addr % NOR_SECTOR_SIZE;

			for(uint32_t i = 0; i < Length; i++)
				*pData++ = *wData++;

			//指针指向Buffer
			pData = nor_buf;

			length -= Length;
			addr   &= AND_NOR_SECTOR;
			Length  = NOR_SECTOR_SIZE;

			//擦除写入位置所在Sector
			sector_erase(addr);

			//写入Buffer
			while(Length > 0)
			{
				page_program(addr, pData, NOR_PAGE_SIZE);

				addr   += NOR_PAGE_SIZE;
				pData  += NOR_PAGE_SIZE;
				Length -= NOR_PAGE_SIZE;
			}
		}
	}
}

static void soft_reset(void)
{
	CS_Low;
	HAL_SPI_Transmit(&hspi1, (uint8_t []){0x66}, 1, HAL_MAX_DELAY);
	CS_High;

	CS_Low;
	HAL_SPI_Transmit(&hspi1, (uint8_t []){0x99}, 1, HAL_MAX_DELAY);
	CS_High;
}

static void init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_InitStruct.Pin   = CS_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    CS_High;

    HAL_GPIO_Init(CS_GPIO, &GPIO_InitStruct);

	soft_reset();
}

void *BSP_NORFLASH_OBJ(void)
{
	static NORFLASH_OBJ obj = {
		.Init      = init,
		.DataRead  = data_read,
		.DataWrite = data_write,
	};

	return &obj;
}
