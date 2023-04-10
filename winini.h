#ifndef __WININI_H__INCLUDED__
#define __WININI_H__INCLUDED__

BOOL iniLoad(const char *filename);
void iniFree();
const char *iniValue(const char *section, const char *variable);
const char *iniLine(const char *section, int line);
const char *iniValueDef(const char *section, const char *variable, const char *defvalue);

typedef BOOL (*linerAction_f)(char *buffer, size_t buffer_size);

typedef struct linerRule
{
	const char *line_match;
	BOOL full_match;
	BOOL last_rule;
	linerAction_f action;
} linerRule_t;

BOOL liner(const char *src, const char *dst, linerRule_t *rules);
BOOL addLine(const char *src, const char *line);

#endif /* __WININI_H__INCLUDED__ */
