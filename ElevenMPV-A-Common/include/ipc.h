#ifndef _ELEVENMPV_IPC_H_
#define _ELEVENMPV_IPC_H_

typedef enum IpcCmd {
	EMPVA_TERMINATE_SHELL_RX,
	EMPVA_IPC_ACTIVATE,
	EMPVA_IPC_DEACTIVATE, 
	EMPVA_IPC_PLAY,
	EMPVA_IPC_FF,
	EMPVA_IPC_REW,
	EMPVA_IPC_INFO,
	EMPVA_IPC_APP_STOP,
	EMPVA_IPC_APP_START
} IpcCmd;

typedef enum IpcFlag {
	EMPVA_IPC_REFRESH_PBBT = 1,
	EMPVA_IPC_REFRESH_TEXT = 2
} IpcFlag;

typedef struct IpcDataRX {
	SceUInt32 cmd;
	SceUInt32 flags;
	SceUInt32 pbbtState;
	SceWChar16 title[256];
	SceWChar16 artist[256];
	SceWChar16 album[256];
} IpcDataRX;

typedef struct IpcDataTX {
	SceUInt32 cmd;
	char data[0x100];
} IpcDataTX;

#endif
