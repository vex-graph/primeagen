#ifndef PRIMEAGEN_TEXT_STYLE_H
#define PRIMEAGEN_TEXT_STYLE_H

/* Terminal-independent, host-owned display attributes; never ANSI byte strings. */
#define TEXT_STYLE_BOLD 0x01u
#define TEXT_STYLE_ITALIC 0x02u
#define TEXT_STYLE_CODE 0x04u
#define TEXT_STYLE_HEADING 0x08u
#define TEXT_STYLE_USER 0x10u
#define TEXT_STYLE_ASSISTANT 0x20u
#define TEXT_STYLE_ERROR 0x40u
#define TEXT_STYLE_MASK 0x7fu

#endif
