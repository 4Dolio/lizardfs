/*
   Copyright 2013-2015 Skytechnology sp. z o.o.

   This file is part of LizardFS.

   LizardFS is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 3.

   LizardFS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with LizardFS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/platform.h"
#include "chunkserver/chunk.h"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <sstream>

#include "common/massert.h"
#include "common/slice_traits.h"

Chunk::Chunk(uint64_t chunkId, ChunkPartType type, ChunkState state, ChunkFormat format)
	: chunkid(chunkId),
	  owner(NULL),
	  version(0),
	  blocks(0),
	  refcount(0),
	  opensteps(0),
	  wasChanged(false),
	  state(state),
	  ccond(NULL),
	  fd(-1),
	  blockExpectedToBeReadNext(0),
	  validattr(0),
	  todel(0),
	  testnext(NULL),
	  testprev(NULL),
	  next(NULL),
	  type_(type),
	  chunkFormat_(format){
}

std::string Chunk::generateFilenameForVersion(uint32_t version, int layout_version) const {
	std::stringstream ss;
	char buffer[30];
	ss << owner->path << Chunk::getSubfolderNameGivenChunkId(chunkid, layout_version) << "/chunk_";
	if (slice_traits::isXor(type_)) {
		if (slice_traits::xors::isXorParity(type_)) {
			ss << "xor_parity_of_";
		} else {
			ss << "xor_" << (unsigned)slice_traits::xors::getXorPart(type_) << "_of_";
		}
		ss << (unsigned)slice_traits::xors::getXorLevel(type_) << "_";
	}
	sprintf(buffer, "%016" PRIX64 "_%08" PRIX32 ".mfs", chunkid, version);
	if (chunkFormat() == ChunkFormat::INTERLEAVED) {
		memcpy(buffer + 26, "liz", 3);
	}
	ss << buffer;
	return ss.str();
}

uint32_t Chunk::maxBlocksInFile() const {
	sassert(slice_traits::isStandard(type_) || slice_traits::isXor(type_));
	if (slice_traits::isStandard(type_)) {
		return MFSBLOCKSINCHUNK;
	} else {
		uint32_t xorLevel = slice_traits::xors::getXorLevel(type_);
		return (MFSBLOCKSINCHUNK + xorLevel - 1) / xorLevel;
	}
}

int Chunk::renameChunkFile(const std::string& newFilename) {
	int status = rename(filename().c_str(), newFilename.c_str());
	if (status < 0) {
		return status;
	}
	setFilename(newFilename);
	return 0;
}

void Chunk::setBlockCountFromFizeSize(off_t fileSize) {
	sassert(isFileSizeValid(fileSize));
	blocks = fileSize / kHddBlockSize;
}

uint32_t Chunk::getSubfolderNumber(uint64_t chunkId, int layout_version) {
	// layout version 0 corresponds to current directory/chunk naming convention
	// values greater than 0 describe older versions (order is not important)
	return (layout_version == 0 ? chunkId >> 16 : chunkId) & 0xFF;
}

std::string Chunk::getSubfolderNameGivenNumber(uint32_t subfolderNumber, int layout_version) {
	sassert(subfolderNumber < Chunk::kNumberOfSubfolders);
	char buffer[16];
	// layout version 0 corresponds to current directory/chunk naming convention
	// values greater than 0 describe older versions (order is not important)
	if (layout_version == 0) {
		sprintf(buffer, "chunks%02X", unsigned(subfolderNumber));
	} else {
		sprintf(buffer, "%02X", unsigned(subfolderNumber));
	}
	return std::string(buffer);
}

std::string Chunk::getSubfolderNameGivenChunkId(uint64_t chunkId, int layout_version) {
	return Chunk::getSubfolderNameGivenNumber(Chunk::getSubfolderNumber(chunkId, layout_version), layout_version);
}

MooseFSChunk::MooseFSChunk(uint64_t chunkId, ChunkPartType type, ChunkState state) :
		Chunk(chunkId, type, state, ChunkFormat::MOOSEFS),
		crc(nullptr),
		crcsteps(0) {
}

off_t MooseFSChunk::getBlockOffset(uint16_t blockNumber) const {
	return getHeaderSize() + MFSBLOCKSIZE * blockNumber;
}

off_t MooseFSChunk::getFileSizeFromBlockCount(uint32_t blockCount) const {
	return getHeaderSize() + blockCount * MFSBLOCKSIZE;
}

void MooseFSChunk::setBlockCountFromFizeSize(off_t fileSize) {
	sassert(isFileSizeValid(fileSize));
	fileSize -= getHeaderSize();
	blocks = fileSize / MFSBLOCKSIZE;
}

bool MooseFSChunk::isFileSizeValid(off_t fileSize) const {
	if (fileSize < static_cast<off_t>(getHeaderSize())) {
		return false;
	}
	fileSize -= getHeaderSize();
	if (fileSize % MFSBLOCKSIZE != 0) {
		return false;
	}
	if (fileSize / MFSBLOCKSIZE > maxBlocksInFile()) {
		return false;
	}
	return true;
}

off_t MooseFSChunk::getSignatureOffset() const {
	return 0;
}

void MooseFSChunk::readaheadHeader() const {
	posix_fadvise(fd, 0, getHeaderSize(), POSIX_FADV_WILLNEED);
}

size_t MooseFSChunk::getHeaderSize() const {
	sassert(slice_traits::isStandard(type_) || slice_traits::isXor(type_));
	if (slice_traits::isStandard(type_)) {
		return kMaxSignatureBlockSize + serializedSize(uint32_t()) * maxBlocksInFile();
	} else {
		uint32_t requiredHeaderSize = kMaxSignatureBlockSize + serializedSize(uint32_t()) * maxBlocksInFile();
		// header size is equal to the requiredHeaderSize rounded up to typical disk block size
		uint32_t diskBlockSize = kDiskBlockSize;
		off_t dataOffset = (requiredHeaderSize + diskBlockSize - 1) / diskBlockSize * diskBlockSize;
		return dataOffset;
	}
}

off_t MooseFSChunk::getCrcOffset() const {
	return kMaxSignatureBlockSize;
}

size_t MooseFSChunk::getCrcBlockSize() const {
	return serializedSize(uint32_t()) * maxBlocksInFile();
}

uint8_t* MooseFSChunk::getCrcBuffer(uint16_t blockNumber) {
	sassert(blockNumber < blocks);
	return crc->data() + (blockNumber * serializedSize(uint32_t()));
}

const uint8_t* MooseFSChunk::getCrcBuffer(uint16_t blockNumber) const {
	sassert(blockNumber < blocks);
	return crc->data() + (blockNumber * serializedSize(uint32_t()));
}

uint32_t MooseFSChunk::getCrc(uint16_t blockNumber) const {
	const uint8_t *ptr = getCrcBuffer(blockNumber);
	return get32bit(&ptr);
}

void MooseFSChunk::initEmptyCrc() {
	crc.reset(new std::array<uint8_t, kMaxCrcBlockSize>);
	memset(crc->data(), 0, getCrcBlockSize());
}

void MooseFSChunk::clearCrc() {
	crc.reset();
	crcsteps = 0;
	if (wasChanged) {
		syslog(LOG_ERR, "serious error: crc changes lost (chunk:%016" PRIX64 "_%08" PRIX32 ")",
				chunkid, version);
	}
}

InterleavedChunk::InterleavedChunk(uint64_t chunkId, ChunkPartType type, ChunkState state) :
		Chunk(chunkId, type, state, ChunkFormat::INTERLEAVED) {
}

off_t InterleavedChunk::getBlockOffset(uint16_t blockNumber) const {
	return static_cast<uint32_t>(blockNumber) * kHddBlockSize;
}

off_t InterleavedChunk::getFileSizeFromBlockCount(uint32_t blockCount) const {
	return blockCount * kHddBlockSize;
}

void InterleavedChunk::setBlockCountFromFizeSize(off_t fileSize) {
	sassert(isFileSizeValid(fileSize));
	blocks = fileSize / kHddBlockSize;
}

bool InterleavedChunk::isFileSizeValid(off_t fileSize) const {
	return fileSize % kHddBlockSize == 0;
}
