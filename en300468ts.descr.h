#define ENDEF_BEGIN(name,vtag) .BR name \ (tag=vtag)
#define ENDEF_END .TP
#define ENDEF_NUMBER0(name) name
#define ENDEF_NUMBER1(name) name
#define ENDEF_DATETIME YYYY/MM/DD HH:MM:SS
#define ENDEF_LOOP LOOP(
#define ENDEF_LOOPEND0 )
#define ENDEF_LOOPEND1 )
#define ENDEF_STRING(name) \N'34'name\N'34'
#define ENDEF_STRING3(name) \N'34'name\N'34' (string length = 3)
