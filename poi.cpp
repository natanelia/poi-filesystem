#include <stdexcept>

#include "poi.h"

/* Global filesystem */
extern POIFS filesystem;


/**                  *
 * BAGIAN KELAS POIFS *
 *                  **/

/** konstruktor */
POIFS::POIFS(){
	time(&mount_time);
}
/** destruktor */
POIFS::~POIFS(){
	handle.close();
}

/** buat file *.poifs baru */
void POIFS::create(const char *filename){

	/* buka file dengan mode input-output, binary dan truncate (untuk membuat file baru) */
	handle.open(filename, fstream::in | fstream::out | fstream::binary | fstream::trunc);

	/* Bagian Volume Information */
	initVolumeInformation(filename);

	/* Bagian Allocation Table */
	initAllocationTable();

	/* Bagian Data Pool */
	initDataPool();

	handle.close();
}
/** inisialisasi Volume Information */
void POIFS::initVolumeInformation(const char *filename) {
	/* buffer untuk menulis ke file */
	char buffer[BLOCK_SIZE];
	memset(buffer, 0, BLOCK_SIZE);

	/* Magic string "POIFS" */
	memcpy(buffer + 0x00, "poi!", 4);

	/* Nama volume */
	if (strcmp(filename,"") == 0) {
        this->filename = "POI!";
	} else {
        this->filename = string(filename);
	}
	memcpy(buffer + 0x04, filename, 32);
	/*memcpy(buffer + 0x04, filename, strlen(filename));*/

	/* Kapasitas filesystem, dalam little endian */
	capacity = N_BLOCK;
	memcpy(buffer + 0x24, (char*)&capacity, 4);

	/* Jumlah blok yang belum terpakai, dalam little endian */
	available = N_BLOCK - 1;
	memcpy(buffer + 0x28, (char*)&available, 4);

	/* Indeks blok pertama yang bebas, dalam little endian */
	firstEmpty = 1;
	memcpy(buffer + 0x2C, (char*)&firstEmpty, 4);

	/* String "SFCC" */
	memcpy(buffer + 0x1FC, "!iop", 4);

	handle.write(buffer, BLOCK_SIZE);
}
/** inisialisasi Allocation Table */
void POIFS::initAllocationTable() {
	short buffer = 0xFFFF;

	/* Allocation Table untuk root */
	handle.write((char*)&buffer, sizeof(short));

	/* Allocation Table untuk lainnya */
	buffer = 0;
	for (int i = 1; i < N_BLOCK; i++) {
		handle.write((char*)&buffer, sizeof(short));
	}
}
/** inisialisasi Data Pool */
void POIFS::initDataPool() {
	char buffer[BLOCK_SIZE];
	memset(buffer, 0, BLOCK_SIZE);

	for (int i = 0; i < N_BLOCK; i++) {
		handle.write(buffer, BLOCK_SIZE);
	}
}

/** baca file .poifs */
void POIFS::load(const char *filename){
	/* buka file dengan mode input-output, dan binary */
	handle.open(filename, fstream::in | fstream::out | fstream::binary);

	/* cek apakah file ada */
	if (!handle.is_open()){
		handle.close();
		throw runtime_error("File not found");
	}

	/* periksa Volume Information */
	readVolumeInformation();

	/* baca Allocation Table */
	readAllocationTable();
}
/** membaca Volume Information */
void POIFS::readVolumeInformation() {
	char buffer[BLOCK_SIZE];
	handle.seekg(0);

	/* Baca keseluruhan Volume Information */
	handle.read(buffer, BLOCK_SIZE);

	/* cek magic string */
	if (string(buffer, 4) != "poi!") {
		handle.close();
		throw runtime_error("File is not a valid POIFS file");
	}

	/* baca capacity */
	memcpy((char*)&capacity, buffer + 0x24, 4);

	/* baca available */
	memcpy((char*)&available, buffer + 0x28, 4);

	/* baca firstEmpty */
	memcpy((char*)&firstEmpty, buffer + 0x2C, 4);
}

/** menuliskan Volume Information */
void POIFS::writeVolumeInformation() {
	handle.seekp(0x00);

	/* buffer untuk menulis ke file */
	char buffer[BLOCK_SIZE];
	memset(buffer, 0, BLOCK_SIZE);

	/* Magic string "POIFS" */
	memcpy(buffer + 0x00, "poi!", 4);

	/* Nama volume */
	memcpy(buffer + 0x04, filename.c_str(), 32);
	/* memcpy(buffer + 0x04, filename.c_str(), filename.length()); */

	/* Kapasitas filesystem, dalam little endian */
	memcpy(buffer + 0x24, (char*)&capacity, 4);

	/* Jumlah blok yang belum terpakai, dalam little endian */
	memcpy(buffer + 0x28, (char*)&available, 4);

	/* Indeks blok pertama yang bebas, dalam little endian */
	memcpy(buffer + 0x2C, (char*)&firstEmpty, 4);

	/* String "SFCC" */
	memcpy(buffer + 0x1FC, "!iop", 4);

	handle.write(buffer, BLOCK_SIZE);
}

/** membaca Allocation Table */
void POIFS::readAllocationTable() {
	char buffer[3];

	/* pindah posisi ke awal Allocation Table */
	handle.seekg(0x200);

	/* baca nilai nextBlock */
	for (int i = 0; i < N_BLOCK; i++) {
		handle.read(buffer, 2);
		memcpy((char*)&nextBlock[i], buffer, 2);
	}
}

/** menuliskan Allocation Table pada posisi tertentu */
void POIFS::writeAllocationTable(ptr_block position) {
	handle.seekp(BLOCK_SIZE + sizeof(ptr_block) * position);
	handle.write((char*)&nextBlock[position], sizeof(ptr_block));
}

/** mengatur Allocation Table */
void POIFS::setNextBlock(ptr_block position, ptr_block next) {
	nextBlock[position] = next;
	writeAllocationTable(position);
}

/** mendapatkan first Empty yang berikutnya */
ptr_block POIFS::allocateBlock() {
	ptr_block result = firstEmpty;

	setNextBlock(result, END_BLOCK);

	while (nextBlock[firstEmpty] != 0x0000) {
		firstEmpty++;
	}
	available--;
	writeVolumeInformation();
	return result;
}

/** membebaskan blok */
void POIFS::freeBlock(ptr_block position) {
	if (position != EMPTY_BLOCK) {
        while (position != END_BLOCK) {
            ptr_block temp = nextBlock[position];
            setNextBlock(position, EMPTY_BLOCK);
            position = temp;
            available--;
        }
        writeVolumeInformation();
	}
}

/** membaca isi block sebesar size kemudian menaruh hasilnya di buf */
int POIFS::readBlock(ptr_block position, char *buffer, int size, int offset) {
	/* kalau sudah di END_BLOCK, return */
	if (position == END_BLOCK) {
		return 0;
	}
	/* kalau offset >= BLOCK_SIZE */
	if (offset >= BLOCK_SIZE) {
		return readBlock(nextBlock[position], buffer, size, offset - BLOCK_SIZE);
	}

	handle.seekg(BLOCK_SIZE * DATA_POOL_OFFSET + position * BLOCK_SIZE + offset);
	int size_now = size;
	/* cuma bisa baca sampai sebesar block size */
	if (offset + size_now > BLOCK_SIZE) {
		size_now = BLOCK_SIZE - offset;
	}
	handle.read(buffer, size_now);

	/* kalau size > block size, lanjutkan di nextBlock */
	if (offset + size > BLOCK_SIZE) {
		return size_now + readBlock(nextBlock[position], buffer + BLOCK_SIZE, offset + size - BLOCK_SIZE);
	}

	return size_now;
}

/** menuliskan isi buffer ke filesystem */
int POIFS::writeBlock(ptr_block position, const char *buffer, int size, int offset) {
	/* kalau sudah di END_BLOCK, return */
	if (position == END_BLOCK) {
		return 0;
	}
	/* kalau offset >= BLOCK_SIZE */
	if (offset >= BLOCK_SIZE) {
		/* kalau nextBlock tidak ada, alokasikan */
		if (nextBlock[position] == END_BLOCK) {
			setNextBlock(position, allocateBlock());
		}
		return writeBlock(nextBlock[position], buffer, size, offset - BLOCK_SIZE);
	}

	handle.seekp(BLOCK_SIZE * DATA_POOL_OFFSET + position * BLOCK_SIZE + offset);
	int size_now = size;
	if (offset + size_now > BLOCK_SIZE) {
		size_now = BLOCK_SIZE - offset;
	}
	handle.write(buffer, size_now);

	/* kalau size > block size, lanjutkan di nextBlock */
	if (offset + size > BLOCK_SIZE) {
		/* kalau nextBlock tidak ada, alokasikan */
		if (nextBlock[position] == END_BLOCK) {
			setNextBlock(position, allocateBlock());
		}
		return size_now + writeBlock(nextBlock[position], buffer + BLOCK_SIZE, offset + size - BLOCK_SIZE);
	}

	return size_now;
}

/**                   *
 * BAGIAN KELAS ENTRY *
 *                   **/

/** Konstruktor: buat Entry kosong */
Entry::Entry() {
	position = 0;
	offset = 0;
	memset(data, 0, ENTRY_SIZE);
}
/** Konstruktor parameter */
Entry::Entry(ptr_block position, unsigned char offset) {
	this->position = position;
	this->offset = offset;

	/* baca dari data pool */
	filesystem.handle.seekg(BLOCK_SIZE * DATA_POOL_OFFSET + position * BLOCK_SIZE + offset * ENTRY_SIZE);
	filesystem.handle.read(data, ENTRY_SIZE);
}

/** Mendapatkan Entry berikutnya */
Entry Entry::nextEntry() {
	if (offset < 15) {
		return Entry(position, offset + 1);
	}
	else {
		return Entry(filesystem.nextBlock[position], 0);
	}
}

/** Mendapatkan Entry dari path */
Entry Entry::getEntry(const char *path) {
	/* mendapatkan direktori teratas */
	unsigned int endstr = 1;
	while (path[endstr] != '/' && endstr < strlen(path)) {
		endstr++;
	}
	string topDirectory = string(path + 1, endstr - 1);

	/* mencari entri dengan nama topDirectory */
	while (getName() != topDirectory && position != END_BLOCK) {
		*this = nextEntry();
	}

	/* kalau tidak ketemu, return Entry kosong */
	if (isEmpty()) {
		return Entry();
	}
	/* kalau ketemu, */
	else {
		if (endstr == strlen(path)) {
			return *this;
		}
		else {
			/* cek apakah direktori atau bukan */
			if (getAttr() & 0x8) {
				ptr_block index;
				memcpy((char*)&index, data + 0x1A, 2);
				Entry next(index, 0);
				return next.getEntry(path + endstr);
			}
			else {
				return Entry();
			}
		}
	}
}

/** Mendapatkan Entry dari path */
Entry Entry::getNewEntry(const char *path) {
	/* mendapatkan direktori teratas */
	unsigned int endstr = 1;
	while (path[endstr] != '/' && endstr < strlen(path)) {
		endstr++;
	}
	string topDirectory = string(path + 1, endstr - 1);

	/* mencari entri dengan nama topDirectory */
	Entry entry(position, offset);
	while (getName() != topDirectory && position != END_BLOCK) {
		*this = nextEntry();
	}

	/* kalau tidak ketemu, buat entry baru */
	if (isEmpty()) {
		while (!entry.isEmpty()) {
			if (entry.nextEntry().position == END_BLOCK) {
				entry = Entry(filesystem.allocateBlock(), 0);
			}
			else {
				entry = entry.nextEntry();
			}
		}
		/* beri atribut pada entry */
		entry.setName(topDirectory.c_str());
		entry.setAttr(0xF);
		entry.setIndex(filesystem.allocateBlock());
		entry.setSize(BLOCK_SIZE);
		entry.setTime(0);
		entry.setDate(0);
		entry.write();

		*this = entry;
	}

	if (endstr == strlen(path)) {
		return *this;
	}
	else {
		/* cek apakah direktori atau bukan */
		if (getAttr() & 0x8) {
			ptr_block index;
			memcpy((char*)&index, data + 0x1A, 2);
			Entry next(index, 0);
			return next.getNewEntry(path + endstr);
		}
		else {
			return Entry();
		}
	}
}

/** Mengembalikan entry kosong selanjutnya. Jika blok penuh, akan dibuatkan entri baru */
Entry Entry::getNextEmptyEntry() {
	Entry entry(*this);

	while (!entry.isEmpty()) {
		entry = entry.nextEntry();
	}
	if (entry.position == END_BLOCK) {
		/* berarti blok saat ini sudah penuh, buat blok baru */
		ptr_block newPosition = filesystem.allocateBlock();
		ptr_block lastPos = position;
		while (filesystem.nextBlock[lastPos] != END_BLOCK) {
			lastPos = filesystem.nextBlock[lastPos];
		}
		filesystem.setNextBlock(lastPos, newPosition);
		entry.position = newPosition;
		entry.offset = 0;
	}

	return entry;
}

/** mengosongkan entry */
void Entry::makeEmpty() {
	/* menghapus byte pertama data */
	*(data) = 0;
	write();
}

/** Memeriksa apakah Entry kosong atau tidak */
int Entry::isEmpty() {
	return *(data) == 0;
}

/** Getter-Setter atribut-atribut Entry */
string Entry::getName() {
	return string(data);
}

unsigned char Entry::getAttr() {
	unsigned char result;
	memcpy(&result, data + 0x15, 1);	
	return result;
}

short Entry::getTime() {
	short result;
	memcpy((char*)&result, data + 0x16, 2);
	return result;
}

short Entry::getDate() {
	short result;
	memcpy((char*)&result, data + 0x18, 2);
	return result;
}

ptr_block Entry::getIndex() {
	ptr_block result;
	memcpy((char*)&result, data + 0x1A, 2);
	return result;
}

int Entry::getSize() {
	int result;
	memcpy((char*)&result, data + 0x1C, 4);
	return result;
}

void Entry::setName(const char* name) {
	strcpy(data, name);
}

void Entry::setAttr(const unsigned char attr) {
	memcpy(data + 0x15, &attr, 1);	
//data[0x15] = attr;
}

void Entry::setTime(const short time) {
	memcpy(data + 0x16, (char*)&time, 2);
}

void Entry::setDate(const short date) {
	memcpy(data + 0x18, (char*)&date, 2);
}

void Entry::setIndex(const ptr_block index) {
	memcpy(data + 0x1A, (char*)&index, 2);
}

void Entry::setSize(const int size) {
	memcpy(data + 0x1C, (char*)&size, 4);
}

/** Bagian Date Time */
time_t Entry::getDateTime() {
	unsigned int datetime;
	memcpy((char*)&datetime, data + 0x16, 4);

	time_t rawtime;
	time(&rawtime);
	struct tm *result = localtime(&rawtime);

	result->tm_sec = datetime & 0x1F;
	result->tm_min = (datetime >> 5u) & 0x3F;
	result->tm_hour = (datetime >> 11u) & 0x1F;
	result->tm_mday = (datetime >> 16u) & 0x1F;
	result->tm_mon = (datetime >> 21u) & 0xF;
	result->tm_year = ((datetime >> 25u) & 0x7F) + 10;

	return mktime(result);
}

void Entry::setCurrentDateTime() {
	time_t now_t;
	time(&now_t);
	struct tm *now = localtime(&now_t);

	int sec = now->tm_sec;
	int min = now->tm_min;
	int hour = now->tm_hour;
	int day = now->tm_mday;
	int mon = now->tm_mon;
	int year = now->tm_year;

	int _time = (sec >> 1) | (min << 5) | (hour << 11);
	int _date = (day) | (mon << 5) | ((year - 10) << 9);

	memcpy(data + 0x16, (char*)&_time, 2);
	memcpy(data + 0x18, (char*)&_date, 2);
}

/** Menuliskan entry ke filesystem */
void Entry::write() {
	if (position != END_BLOCK) {
		filesystem.handle.seekp(BLOCK_SIZE * DATA_POOL_OFFSET + position * BLOCK_SIZE + offset * ENTRY_SIZE);
		filesystem.handle.write(data, ENTRY_SIZE);
	}
}
