#ifndef WOS_MEMORY_H
#define WOS_MEMORY_H

int wos_memory_edit(const char *path);
int wos_memory_view(const char *path);
int wos_memory_push(void);

int wos_memory_tag(const char *path, const char *tags, int nested);
int wos_memory_note(const char *path, const char *note, int nested);
int wos_memory_summary(const char *path, const char *summary, int nested);
int wos_memory_custom(const char *path, const char *field, const char *values, int nested);

#endif
