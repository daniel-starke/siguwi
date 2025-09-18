/**
 * @file utf8.c
 * @author Daniel Starke
 * @see utf8.h
 * @date 2025-07-17
 * @version 2025-07-17
 */
#include "utf8.h"


/**
 * Parses a byte of a UTF-8 byte stream. Returns a code point if found.
 *
 * @param[in,out] ctx - parser context
 * @param[in] b - input byte
 * @return `UTF8_MORE` if more data is needed, otherwise the parsed code point
 * @remarks `UTF8_ERROR` is returned on parsing error
 */
uint32_t utf8_parse(tUtf8Ctx * ctx, uint8_t b) {
	if (ctx->rem == 0) {
		if (b <= 0x7F) {
			return b;
		} else if ((b & 0xE0) == 0xC0) {
			ctx->cp = b & 0x1F;
			ctx->rem = 1;
		} else if ((b & 0xF0) == 0xE0) {
			ctx->cp = b & 0x0F;
			ctx->rem = 2;
		} else if ((b & 0xF8) == 0xF0) {
			ctx->cp = b & 0x07;
			ctx->rem = 3;
		} else {
			return UTF8_ERROR;
		}
	} else {
		if ((b & 0xC0) != 0x80) {
			ctx->rem = 0;
			return UTF8_ERROR;
		}
		ctx->cp = (ctx->cp << 6) | (b & 0x3F);
		--(ctx->rem);
		if (ctx->rem == 0) {
			return ctx->cp;
		}
	}
	return UTF8_MORE;
}
