#ifndef  __APP_CCTV_H__
#define  __APP_CCTV_H__

#ifdef CONFIG_AEOLIAN_CCTV_CTRL

void cctv_task(void* pvParameters);
void toggle_cctv_daynight(uint16_t devId);

#endif /* CONFIG_AEOLIAN_CCTV_CTRL */
#endif /* __APP_CCTV_H__ */