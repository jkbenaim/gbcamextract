#ifndef _SRAM_H_
#define _SRAM_H_

#include <inttypes.h>

struct image_metadata_s {
	uint32_t userid;
	uint8_t username[9];
	uint8_t blood_sex;	// 0h = no gender, 1h = male, 2h = female, 4h = A, 8h = B, Ch = O, 10h = AB
	uint32_t birthdate;
	uint8_t pad12[3];	// unknown content
	uint8_t comment[0x30-0x15];
	uint8_t pad30[3];	// always 00h
	uint8_t copied;		// 00h = original, 01h = copy
	uint16_t checksum2;	// maybe?
	uint8_t pad36[0x54-0x36];	// always 00h
	uint8_t border;
	uint8_t magic[5];	// ASCII "Magic" without null byte
	uint16_t checksum;
} __attribute__((packed));

struct user_metadata_s {
	uint32_t userid;
	uint8_t username[9];
	uint8_t blood_sex;
	uint32_t birhdate;
	uint8_t magic[5]; // ASCII "Magic" without null byte
	uint16_t checksum;
} __attribute__((packed));

struct slot_s {
	uint8_t image[0xE00];
	uint8_t thumbnail[0x100];
	struct image_metadata_s imagemeta;
	struct image_metadata_s imagemeta2;
	struct user_metadata_s	usermeta;
	struct user_metadata_s	usermeta2;
	uint8_t padFEA[17];	// always AAh
	uint8_t padFFA[5];	// trash
} __attribute__((packed));

struct firstslot_s {
	uint8_t image[0x1000];
	uint8_t pad[0x11b2-0x1000];
	uint8_t vec[0x11d0-0x11b2];
	uint8_t magic[5];	// ASCI "Magic" without null byte
	uint16_t checksum;
} __attribute__((packed));

#endif

