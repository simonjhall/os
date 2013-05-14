#include "elf.h"
#include "common.h"

#include <string.h>

Elf::Elf()
{
	m_initialised = false;
	m_littleEndian = true;
}

//bool Elf::Load(const char *filename)
//{
//	FILE *fp = fopen(filename, "rb");
//	if (!fp)
//		return false;
//
//	int ok = fseek(fp, 0, SEEK_END);
//	ASSERT(ok == 0);
//
//	unsigned int length = ftell(fp);
//	rewind(fp);
//
//	m_pElf = malloc(length);
//	fread(m_pElf, length, 1, fp);
//	fclose(fp);
//
//	ok = Load(m_pElf, length);
//
//	m_allocated = true;
//
//	return ok ? true : false;
//}

bool Elf::Load(void *base, unsigned int size)
{
	m_pHeader = (Elf32_Ehdr *)base;

	if (m_pHeader->e_ident[0] != 0x7f
			|| m_pHeader->e_ident[1] != 'E'
			|| m_pHeader->e_ident[2] != 'L'
			|| m_pHeader->e_ident[3] != 'F')
	{
		//no elf header
		return false;
	}

	if (m_pHeader->e_ident[5] != 2)		//little endian
		m_littleEndian = true;
	else								//big-endian
		m_littleEndian = false;

	m_pHeader->e_type = swap_bytes(m_pHeader->e_type, m_littleEndian);
	m_pHeader->e_machine = swap_bytes(m_pHeader->e_machine, m_littleEndian);
	m_pHeader->e_version = swap_bytes(m_pHeader->e_version, m_littleEndian);
	m_pHeader->e_entry = swap_bytes(m_pHeader->e_entry, m_littleEndian);
	m_pHeader->e_phoff = swap_bytes(m_pHeader->e_phoff, m_littleEndian);
	m_pHeader->e_shoff = swap_bytes(m_pHeader->e_shoff, m_littleEndian);
	m_pHeader->e_flags = swap_bytes(m_pHeader->e_flags, m_littleEndian);
	m_pHeader->e_ehsize = swap_bytes(m_pHeader->e_ehsize, m_littleEndian);
	m_pHeader->e_phentsize = swap_bytes(m_pHeader->e_phentsize, m_littleEndian);
	m_pHeader->e_phnum = swap_bytes(m_pHeader->e_phnum, m_littleEndian);
	m_pHeader->e_shentsize = swap_bytes(m_pHeader->e_shentsize, m_littleEndian);
	m_pHeader->e_shnum = swap_bytes(m_pHeader->e_shnum, m_littleEndian);
	m_pHeader->e_shstrndx = swap_bytes(m_pHeader->e_shstrndx, m_littleEndian);

	if (m_pHeader->e_type != 2)
	{
		//not an executable elf format
		return false;
	}

	if (m_pHeader->e_shstrndx == 0)
	{
		//no section name string table
		return false;
	}

	if (m_pHeader->e_phoff == 0)
	{
		//no program header table offset
		return false;
	}

	if (m_pHeader->e_shoff == 0)
	{
		//no section header table offset
		return false;
	}

	m_pSectionHeader = (Elf32_Shdr *)&((char *)base)[m_pHeader->e_shoff];
	m_pProgramHeader = (Elf32_Phdr *)&((char *)base)[m_pHeader->e_phoff];
	m_pSectionStrings = &((char *)base)[swap_bytes(m_pSectionHeader[m_pHeader->e_shstrndx].sh_offset, m_littleEndian)];

//	for (unsigned int count = 0; count < m_pHeader->e_shnum; count++)
//		printf("section %d: %s\n", count, &m_pSectionStrings[swap_bytes(m_pSectionHeader[count].sh_name, m_littleEndian)]);

	m_initialised = true;
	m_pElf = base;

	return true;
}

void *Elf::FindSection(const char *name, unsigned int *length, unsigned int *vaddr)
{
	if (!m_initialised)
		return 0;

	for (unsigned int count = 0; count < m_pHeader->e_shnum; count++)
		if (strcmp(name, &m_pSectionStrings[swap_bytes(m_pSectionHeader[count].sh_name, m_littleEndian)]) == 0)
		{
			if (length)
				*length = swap_bytes(m_pSectionHeader[count].sh_size, m_littleEndian);

			if (vaddr)
				*vaddr = swap_bytes(m_pSectionHeader[count].sh_addr, m_littleEndian);

			return (void *)&((char *)m_pElf)[swap_bytes(m_pSectionHeader[count].sh_offset, m_littleEndian)];
		}

	return 0;
}

void *Elf::GetEntryPoint(void)
{
	if (m_initialised)
		return (void *)m_pHeader->e_entry;
	else
		return 0;
}

unsigned int Elf::GetNumProgramHeaders(void)
{
	if (m_initialised)
		return m_pHeader->e_phnum;
	else
		return 0;
}

bool Elf::GetProgramHeader(int i, void **data, unsigned int *vaddr, unsigned int *memsize, unsigned int *filesize)
{
	if (i < 0 || (unsigned int)i >= GetNumProgramHeaders())
		return false;

	Elf32_Phdr *p = &m_pProgramHeader[i];

	*data = (void *)&((char *)m_pElf)[swap_bytes(p->p_offset, m_littleEndian)];
	*vaddr = swap_bytes(p->p_vaddr, m_littleEndian);
	*memsize= swap_bytes(p->p_memsz, m_littleEndian);
	*filesize = swap_bytes(p->p_filesz, m_littleEndian);

	if (swap_bytes(p->p_type, m_littleEndian) == 1)
		return true;
	else
		return false;
}

Elf::~Elf()
{
	if (m_initialised && m_allocated)
		delete[] (unsigned char *)m_pElf;
}
