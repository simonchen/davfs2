#if !defined(_RPMATCH_H_)
#define _RPMATCH_H_

/*
 * Determine whether the string value of RESPONSE matches the affirmation
 * or negative response expression as specified by the LC_MESSAGES category
 * in the program's current locale.  Returns 1 if affirmative, 0 if
 * negative, and -1 if not matching.
 */
extern int rpmatch (const char *response);

#endif /* !defined(_RPMATCH_H_) */