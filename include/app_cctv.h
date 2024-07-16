#ifndef  __APP_CCTV_H__
#define  __APP_CCTV_H__

#if defined (CONFIG_CCTV)

void cctv_task(void* pvParameters);
void toggle_cctv_daynight(uint16_t devId);

#endif /* CONFIG_CCTV */
#endif /* __APP_CCTV_H__ */