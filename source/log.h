/**

         This method is created by Jolty (umbjolt in gbatemp) to help debugging DSIMenuPlusPlus
         
/**

#ifndef LOG_H
#define LOG_H

#define LOG_PATH "sdmc:/_nds/logs/log.ini"

extern bool LogCreated;

int createLog(void);
void Log(const char *message);
void LogFM(const char *from, const char *message);
void LogFMA(const char *from, const char *message, const char *additional_info);
#endif // LOG_H
