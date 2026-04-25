/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "SoundSet.h"
#include "CatFile.h"
#include "Sound.h"
#include "Logger.h"
#include "SDL2Helpers.h"
#include "FileMap.h"
#include <climits>
#include <cassert>

namespace OpenXcom
{

/**
 * Sets up a new empty sound set.
 */
SoundSet::SoundSet() : _sharedSounds(INT_MAX)
{

}

/**
 * Converts a 8Khz sample to 11Khz.
 * @param oldsound Pointer to original sample buffer.
 * @param oldsize Original buffer size.
 * @param newsound Pointer to converted sample buffer.
 * @return Converted buffer size.
 */
int SoundSet::convertSampleRate(Uint8 *oldsound, size_t oldsize, Uint8 *newsound) const
{
	const Uint32 step16 = (8000 << 16) / 11025;
	int newsize = 0;
	for (Uint32 offset16 = 0; (offset16 >> 16) < oldsize; offset16 += step16, ++newsound, ++newsize)
	{
		*newsound = oldsound[offset16 >> 16];
	}
	return newsize;
}

/* 16-bit signed PCM mono at 11025 Hz.
 * decodeAudioData (Web Audio API) rejects 8-bit PCM on some engines.
 * ByteRate = SampleRate * BlockAlign = 11025 * 2 = 22050 = 0x5622.
 * BlockAlign = 2, BitsPerSample = 16. */
static const Uint8 header[] = {  'R',  'I',  'F',  'F', 0x00, 0x00, 0x00, 0x00,  'W',  'A',  'V',  'E',
								 'f',  'm',  't',  ' ', 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
								0x11, 0x2b, 0x00, 0x00, 0x22, 0x56, 0x00, 0x00, 0x02, 0x00, 0x10, 0x00,
								 'd',  'a',  't',  'a', 0x00, 0x00, 0x00, 0x00                           };
/**
 * Write out a WAV. Resample if needed.
 * @param dest where to write
 * @param sound sound data
 * @param size  size of sound data
 * @param resample if resampling is needed.
 */
void SoundSet::writeWAV(SDL_RWops *dest, Uint8 *sound, size_t size, bool resample) const {
	SDL_RWwrite(dest, header, sizeof(header), 1);

	/* Expand 8-bit unsigned PCM → 16-bit signed PCM.
	 * formula: s16 = (u8 - 128) * 256  (preserves silence at 0x80). */
	auto expand16 = [](Uint8 *src, int count) -> Sint16* {
		auto out = (Sint16*)SDL_malloc((size_t)count * 2);
		for (int i = 0; i < count; ++i)
			out[i] = ((Sint16)src[i] - 128) * 256;
		return out;
	};

	int newsize;
	if (resample) {
		auto buf8 = (Uint8*)SDL_malloc(2*size);
		int resampled = convertSampleRate(sound, size, buf8);
		Sint16 *out = expand16(buf8, resampled);
		SDL_free(buf8);
		newsize = resampled * 2;
		SDL_RWwrite(dest, out, newsize, 1);
		SDL_free(out);
	} else {
		Sint16 *out = expand16(sound, (int)size);
		newsize = (int)size * 2;
		SDL_RWwrite(dest, out, newsize, 1);
		SDL_free(out);
	}

	// update the header
	SDL_RWseek(dest, 4, RW_SEEK_SET);
	SDL_WriteLE32(dest, newsize + 36);
	SDL_RWseek(dest, 40, RW_SEEK_SET);
	SDL_WriteLE32(dest, newsize);
	// Restore position to end of written data so callers can read bytes_written
	// via SDL_RWseek(dest, 0, SEEK_CUR) and get the full WAV size, not just 44.
	SDL_RWseek(dest, 44 + newsize, RW_SEEK_SET);
}

/**
 * Loads the contents of an X-Com CAT file which usually contains
 * a set of sound files. The CAT starts with an index of the offset
 * and size of every file contained within. Each file consists of a
 * filename followed by its contents.
 * @param rw RWops of the CAT set.
 * @param wav Are the sounds in WAV format?
 * @sa http://www.ufopaedia.org/index.php?title=SOUND
 */
void SoundSet::loadCat(CatFile &catFile)
{
	for (size_t i = 0; i < catFile.size(); ++i) { loadCatByIndex(catFile, i); }
}

/**
 * Returns a particular wave from the sound set.
 * @param i Sound number in the set.
 * @return Pointer to the respective sound.
 */
Sound *SoundSet::getSound(int i)
{
	if (_sounds.find(i) != _sounds.end())
	{
		return &_sounds[i];
	}
	return 0;
}

/**
 * Creates and returns a particular wave in the sound set.
 * @param i Sound number in the set.
 * @return Pointer to the respective sound.
 */
Sound *SoundSet::addSound(int i)
{
	assert(i >= 0 && "Negative indexes are not supported in SoundSet");
	_sounds[i] = Sound();
	return &_sounds[i];
}

/**
 * Set number of shared sound indexes that are accessible for all mods.
 */
void SoundSet::setMaxSharedSounds(int i)
{
	if (i >= 0)
	{
		_sharedSounds = i;
	}
	else
	{
		_sharedSounds = 0;
	}
}

/**
 * Gets number of shared sound indexes that are accessible for all mods.
 */
int SoundSet::getMaxSharedSounds() const
{
	return _sharedSounds;
}

/**
 * Returns the total amount of sounds currently
 * stored in the set.
 * @return Number of sounds.
 */
size_t SoundSet::getTotalSounds() const
{
	return _sounds.size();
}

/**
 * Loads individual contents of a sound CAT file by index.
 * a set of sound files. The CAT starts with an index of the offset
 * and size of every file contained within. Each file consists of a
 * filename followed by its contents.
 * @param filename Filename of the CAT set.
 * @param index which index in the cat file do we load?
 * @param tftd if to expect signed 8bit 11Khz instead of unsigned 6bit 8KHz in the data.
 *             and also under which ID to put the sound
 * @sa http://www.ufopaedia.org/index.php?title=SOUND
 */
void SoundSet::loadCatByIndex(CatFile &catFile, int index, bool tftd)
{
	int set_index = tftd ? getTotalSounds() : index;
	_sounds[set_index] = Sound(); // in case everything else fails, an empty Sound.
	auto rwops = catFile.getRWops(index);
	if (!rwops) {
		Log(LOG_VERBOSE) << "SoundSet::loadCatByIndex(" << catFile.fileName() << ", " << index << "): got NULL.";
		return;
	}
	int namesize = SDL_ReadU8(rwops);
	SDL_RWseek(rwops, namesize, RW_SEEK_CUR); // skip "name".
	// NB: the original code after ce548c29d5742e26a442a44ef2a5fcce3f80dace
	// did not adjust the size for the namesize byte when skipping the name
	// and thus submitted one trailing byte of garbage.
	// The original code before that commit did not adjust the size at all
	// when skipping the name and thus submitted at least one trailing byte of garbage.
	// v1.4 sounds do miss namesize+1 bytes as they are in the catfile -
	// comparing what's in the WAV header to cat item size without name.

	size_t size;
	Uint8 *sound = (Uint8 *)SDL_LoadFile_RW(rwops, &size, SDL_TRUE);

	// Skip short data
	if (size < 12) {
		Log(LOG_VERBOSE) << "SoundSet::loadCatByIndex(" << catFile.fileName() << ", " << index << ") size=" << size <<" , skipping.";
		SDL_free(sound);
		return;
	}

	// See if we've got RIFF header here.
	bool wav = ((sound[0] == 'R') && (sound[1] == 'I') && (sound[2]  == 'F') && (sound[3]  == 'F')
			 && (sound[8] == 'W') && (sound[9] == 'A') && (sound[10] == 'V') && (sound[11] == 'E'));

	Uint8 *samples;
	size_t samplecount;
	bool do_resample = true;
	if (wav) { // skip WAV header
		int expected_size = *(Sint32 *)(sound +0x04) + 8;
		int delta = ((int)size) - expected_size;
		// fix the header if we miss some data.
		if (delta < 0) {
			*(Sint32 *)(sound +0x04) += delta; // WAVE chunk size
			*(Sint32 *)(sound +0x28) += delta; // data chunk size
		}
		int samplerate = *(Sint32 *)(sound + 0x18);
		do_resample  = (samplerate < 11025);
		samples = sound + 44;
		samplecount = size - 44;
	} else { // skip DOS header
		// UFO2000 style
		samples = sound + 6;
		samplecount = size - 6;

		// OpenXcom style
		samples = sound + 5;
		samplecount = size - 6;

		// scale to 8 bits (UFO) or get rid of signedness (TFTD)
		for (size_t n = 0; n < samplecount; ++n) {
			int sample = samples[n];
			samples[n] = (Uint8) (tftd ? sample + 128 : sample * 4);
		}
	}
	size_t dest_size = 44 + 4 * size; // 16-bit resampled worst case: 2× sample count × 2 bytes
	auto dest_mem = SDL_malloc(dest_size);
#ifdef __EMSCRIPTEN__
	// SDL_RWFromMem returns a JS rwops id; C macros (SDL_RWwrite/seek) need a C struct.
	// Use em_writable_mem_to_rwops for the write phase, then a fresh JS id for Mix_LoadWAV_RW.
	{
		auto c_rwops = em_writable_mem_to_rwops(dest_mem, dest_size);
		if (do_resample) {
			writeWAV(c_rwops, samples, samplecount, !tftd);
		} else {
			SDL_RWwrite(c_rwops, sound, size, 1);
		}
		long bytes_written = SDL_RWseek(c_rwops, 0, RW_SEEK_CUR);
		SDL_RWclose(c_rwops);
		auto dest_rwops = SDL_RWFromMem(dest_mem, (int)bytes_written);
		_sounds[set_index].load(dest_rwops);
	}
#else
	{
		auto dest_rwops = SDL_RWFromMem(dest_mem, dest_size);
		if (do_resample) {
			writeWAV(dest_rwops, samples, samplecount, !tftd);
		} else {
			SDL_RWwrite(dest_rwops, sound, size, 1);
		}
		SDL_RWseek(dest_rwops, 0, RW_SEEK_SET);
		_sounds[set_index].load(dest_rwops);
	}
#endif
	SDL_free(dest_mem);
	SDL_free(sound);
}

}
