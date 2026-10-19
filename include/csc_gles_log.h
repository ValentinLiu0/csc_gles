#ifndef CSC_GLES_LOG_H
#define CSC_GLES_LOG_H

#define LOG_V(fmt, ...) fprintf(stdout, "[V] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define LOG_D(fmt, ...) fprintf(stdout, "[D] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define LOG_I(fmt, ...) fprintf(stdout, "[I] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define LOG_W(fmt, ...) fprintf(stdout, "[W] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define LOG_E(fmt, ...) fprintf(stderr, "[E] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
#define LOG_F(fmt, ...) fprintf(stderr, "[F] %s@%s[:%d]: " fmt, __func__, __FILE_NAME__, __LINE__, ##__VA_ARGS__)

#endif // CSC_GLES_LOG_H
