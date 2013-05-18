#ifndef ELF_H_
#define ELF_H_

#include <elf.h>

class Elf
{
public:

	enum Endian
	{
		kBig,
		kLittle
	};

public:
	Elf();
	virtual ~Elf();

	bool Load(const char *filename);
	bool Load(void *base, unsigned int size);

	void *FindSection(const char *, unsigned int *length, unsigned int *vaddr);
	void *GetEntryPoint(void);
	unsigned int GetNumProgramHeaders(void);
	unsigned int GetProgramHeader(int i, void **data, unsigned int *vaddr, unsigned int *memsize, unsigned int *filesize);
	void *GetWholeImage(void) { return m_pElf; };
	Endian GetEndianness(void) { if (m_pHeader->e_ident[5] != 2) return kLittle; else return kBig; }

private:
	bool m_initialised;
	bool m_allocated;
	bool m_littleEndian;

	void *m_pElf;

	Elf32_Ehdr *m_pHeader;
	Elf32_Shdr *m_pSectionHeader;
	Elf32_Phdr *m_pProgramHeader;
	char *m_pSectionStrings;

public:
	static unsigned char swap_bytes(unsigned char a, bool)
	{
		return a;
	};
	static unsigned short swap_bytes(unsigned short a, bool elfEndianness)
	{
		if (elfEndianness)
			return a;
		else
			return ((a >> 8) & 0xff) | (a << 8);
	};
	static unsigned int swap_bytes(unsigned int a, bool elfEndianness)
	{
		if (elfEndianness)
			return a;
		else
		{
			union {
				unsigned int i;
				struct {
					unsigned char b[4];
				};
			} swap_in, swap_out;

			swap_in.i = a;
			swap_out.b[0] = swap_in.b[3];
			swap_out.b[1] = swap_in.b[2];
			swap_out.b[2] = swap_in.b[1];
			swap_out.b[3] = swap_in.b[0];

			return swap_out.i;
		}
	};


};

#endif /*ELF_H_*/
