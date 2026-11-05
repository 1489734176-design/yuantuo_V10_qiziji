#include "flash_data_save.h"
#include "user_control.h"
#include "tool_mode.h"

/* 末两页为事务日志，工程ROM上限必须为30KB，禁止代码落入此区域。 */
#define FLASH_PAGE0_ADDRESS (0x08007800ul)
#define FLASH_PAGE1_ADDRESS (0x08007c00ul)
#define FLASH_PAGE_BYTES    (1024u)
#define FLASH_SLOT_BYTES    (20u)
#define FLASH_SLOT_COUNT    (FLASH_PAGE_BYTES/FLASH_SLOT_BYTES)
#define FLASH_RECORD_WORDS  (8u)
#define FLASH_RECORD_MAGIC (0xa65au)
#define FLASH_RECORD_VERSION (1u)
#define FLASH_LEGACY_MAGIC  (0x55aau)
#define FLASH_LEGACY_SLOTS  (10u)

/* 新记录8半字：最后提交标志、版本、模式、调速档、情景档、序号低/高、CRC16。 */
typedef struct
{
	uint32_t page;
	uint32_t sequence;
	uint16_t record[FLASH_RECORD_WORDS];
	uint8_t valid;
	uint8_t legacy;
	uint8_t legacy_auto_stop;
} flash_latest_t;

/**
 * @brief 从指定Flash地址读取一个半字。
 */
uint16_t read_half_data(uint32_t address)
{
	return *(volatile uint16_t *)address;
}

/**
 * @brief 计算记录版本、模式、挡位及序号的CRC16-CCITT，不包含提交标志。
 */
static uint16_t flash_record_crc(const uint16_t *record)
{
	uint16_t crc=0xffffu;
	uint8_t word;
	uint8_t byte;
	uint8_t bit;
	/* CRC16-CCITT，按每半字低字节在先覆盖版本至32位序号，标志不参与。 */
	for(word=1;word<7;word++)
	{
		for(byte=0;byte<2;byte++)
		{
			crc^=(uint16_t)(((record[word]>>(8u*byte)) & 0xffu)<<8);
			for(bit=0;bit<8;bit++)
			{
				crc=(crc & 0x8000u) ? (uint16_t)((crc<<1)^0x1021u) : (uint16_t)(crc<<1);
			}
		}
	}
	return crc;
}

/**
 * @brief 读取并校验一条日志，只有提交标志、版本、范围和CRC均有效才返回1。
 */
static uint8_t flash_read_record(uint32_t address,uint16_t *record)
{
	uint8_t i;
	if(read_half_data(address)!=FLASH_RECORD_MAGIC) return 0;
	for(i=0;i<FLASH_RECORD_WORDS;i++) record[i]=read_half_data(address+2u*i);
	/* CRC和范围同时检查，损坏/旧格式/未完成记录均不成为最新设置。 */
	return record[1]==FLASH_RECORD_VERSION && record[2]<=TOOL_MODE_SCENE &&
		record[3]>=1 && record[3]<=4 && record[4]>=1 && record[4]<=3 &&
		record[7]==flash_record_crc(record);
}

/**
 * @brief 扫描双页事务日志获取最新记录，无新格式日志时兼容读取旧挡位记录。
 */
static void flash_find_latest(flash_latest_t *latest)
{
	uint32_t page;
	uint32_t address;
	uint32_t sequence;
	uint16_t record[FLASH_RECORD_WORDS];
	uint8_t page_index;
	uint8_t slot;
	uint8_t i;
	latest->valid=0;
	latest->legacy=0;
	latest->sequence=0;
	latest->legacy_auto_stop=0;
	for(page_index=0;page_index<2;page_index++)
	{
		page=(page_index==0) ? FLASH_PAGE0_ADDRESS : FLASH_PAGE1_ADDRESS;
		for(slot=0;slot<FLASH_SLOT_COUNT;slot++)
		{
			address=page+(uint32_t)slot*FLASH_SLOT_BYTES;
			if(!flash_read_record(address,record)) continue;
			sequence=(uint32_t)record[5] | ((uint32_t)record[6]<<16);
			/* 无效槽不截断扫描；32位序号按差值处理自然回绕。 */
			if(!latest->valid || (int32_t)(sequence-latest->sequence)>0)
			{
				latest->valid=1;
				latest->page=page;
				latest->sequence=sequence;
				for(i=0;i<FLASH_RECORD_WORDS;i++) latest->record[i]=record[i];
			}
		}
	}
	if(latest->valid) return;
	/* 仅无新日志时读取旧页31首10槽；旧0/1挡位转换为调速1/2，自动停机仍兼容读取。 */
	for(slot=0;slot<FLASH_LEGACY_SLOTS;slot++)
	{
		address=FLASH_PAGE1_ADDRESS+(uint32_t)slot*FLASH_SLOT_BYTES;
		if(read_half_data(address)==FLASH_LEGACY_MAGIC &&
		   read_half_data(address+2)<=1 && read_half_data(address+4)<=1)
		{
			latest->valid=1;
			latest->legacy=1;
			latest->page=FLASH_PAGE1_ADDRESS;
			latest->record[2]=TOOL_MODE_SPEED;
			latest->record[3]=read_half_data(address+2)+1u;
			latest->record[4]=1;
			latest->legacy_auto_stop=(uint8_t)read_half_data(address+4);
		}
	}
}

/**
 * @brief 查找完整擦净的20字节日志槽，整页无空槽时返回0。
 */
static uint32_t flash_find_empty_slot(uint32_t page)
{
	uint8_t slot;
	uint8_t word;
	uint32_t address;
	for(slot=0;slot<FLASH_SLOT_COUNT;slot++)
	{
		address=page+(uint32_t)slot*FLASH_SLOT_BYTES;
		/* 整个20字节必须擦净，残缺记录不能再次编程，只跳过不覆盖。 */
		for(word=0;word<FLASH_SLOT_BYTES/2u;word++)
		{
			if(read_half_data(address+2u*word)!=0xffffu) break;
		}
		if(word==FLASH_SLOT_BYTES/2u) return address;
	}
	return 0;
}

/**
 * @brief 以双页事务日志保存模式和挡位，最后提交标志并读回校验；成功返回1。
 *        调用者须确认电机完全停止，任意擦写或校验失败均保留上次有效记录。
 */
uint8_t save_user_data_checked(void)
{
	flash_latest_t latest;
	uint16_t record[FLASH_RECORD_WORDS];
	uint16_t verify[FLASH_RECORD_WORDS];
	uint32_t page;
	uint32_t address;
	uint32_t sequence;
	uint8_t erase_needed=0;
	uint8_t i;
	uint8_t result=0;
	uint16_t erase_word;
	/* 拒绝无效RAM配置，失败不清待保存标志。调用者保证前台完全停机。 */
	if(tool_mode_list.mode>TOOL_MODE_SCENE || tool_mode_list.speed_gear<1 ||
	   tool_mode_list.speed_gear>4 || tool_mode_list.scene_gear<1 || tool_mode_list.scene_gear>3)
	{
		return 0;
	}
	flash_find_latest(&latest);
	page=(latest.valid && !latest.legacy) ? latest.page : FLASH_PAGE0_ADDRESS;
	address=flash_find_empty_slot(page);
	if(address==0)
	{
		/* 写满后仅擦另一页，最新有效页保留到新记录完整写入且读回成功。 */
		if(latest.valid && !latest.legacy)
		{
			page=(latest.page==FLASH_PAGE0_ADDRESS) ? FLASH_PAGE1_ADDRESS : FLASH_PAGE0_ADDRESS;
		}
		erase_needed=1;
		address=page;
	}
	sequence=latest.valid ? latest.sequence+1u : 1u;
	record[0]=FLASH_RECORD_MAGIC;
	record[1]=FLASH_RECORD_VERSION;
	record[2]=tool_mode_list.mode;
	record[3]=tool_mode_list.speed_gear;
	record[4]=tool_mode_list.scene_gear;
	record[5]=(uint16_t)sequence;
	record[6]=(uint16_t)(sequence>>16);
	record[7]=flash_record_crc(record);
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	if(erase_needed)
	{
		if(FLASH_ErasePage(page)!=FLASH_COMPLETE) goto save_done;
		/* 擦页成功还须整页读回FFFF，避免残留旧magic使后续半写误成有效记录。 */
		for(erase_word=0;erase_word<FLASH_PAGE_BYTES/2u;erase_word++)
		{
			if(read_half_data(page+2u*erase_word)!=0xffffu) goto save_done;
		}
	}
	/* 先写数据并逐半字读回；最后写magic提交，任何失败都留下可跳过的残缺槽。 */
	for(i=1;i<FLASH_RECORD_WORDS;i++)
	{
		if(FLASH_ProgramHalfWord(address+2u*i,record[i])!=FLASH_COMPLETE ||
		   read_half_data(address+2u*i)!=record[i]) goto save_done;
	}
	if(FLASH_ProgramHalfWord(address,record[0])!=FLASH_COMPLETE) goto save_done;
	if(!flash_read_record(address,verify)) goto save_done;
	for(i=0;i<FLASH_RECORD_WORDS;i++)
	{
		if(verify[i]!=record[i]) goto save_done;
	}
	result=1;
save_done:
	FLASH_Lock();
	return result;
}

/**
 * @brief 兼容旧保存接口，标记待保存并通过同一前台停机门控提交。
 */
void save_user_data(void)
{
	/* 保留旧调用接口，同样经停机门控；失败由待保存标志留待后续重试。 */
	tool_mode_list.save_pending=1;
	tool_mode_save_handle();
}

/**
 * @brief 上电恢复最新有效模式和挡位，无有效记录时使用默认设置并避免写Flash。
 */
void read_user_data(void)
{
	flash_latest_t latest;
	/* 空页或全部损坏时采用默认，不在启动期间重写默认设置。 */
	tool_mode_init();
	flash_find_latest(&latest);
	if(latest.valid)
	{
		tool_mode_list.mode=(uint8_t)latest.record[2];
		tool_mode_list.speed_gear=(uint8_t)latest.record[3];
		tool_mode_list.scene_gear=(uint8_t)latest.record[4];
	}
	user_list.gears_level=tool_mode_gear()-1;
	user_list.flag.bits.gAutoStopEn=latest.legacy_auto_stop;
}
