/*********************************************************************
* Copyright (C) Anton Kovalev (vertver), 2019. All rights reserved.
* Fresponze - fast, simple and modern multimedia sound library
* Apache-2 License
**********************************************************************
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*****************************************************************/
#include "FresponzeWavFile.h"


CRIFFMediaResource::CRIFFMediaResource(IFreponzeMapFile* pNewMapper)
{ 
	pNewMapper->Clone((void**)&pMapper);
}

CRIFFMediaResource::~CRIFFMediaResource()
{
	CloseResource();
}

bool
CRIFFMediaResource::OpenResource(void* pResourceLinker)
{
	fr_u32 cmpdword = 0x46464952;
	wav_header* wavHeader = nullptr;

	/* Open mapped file in read-only mode */
	if (!pMapper->Open((const fr_utf8*)pResourceLinker, eReadFlag | eMustExistFlag)) return false;
	if (!pMapper->MapPointer(sizeof(wav_header), (fr_ptr&)wavHeader, 0, eMappingRead)) {
		pMapper->Close();
		return false;
	}
	
	/* Compare magic RIFF word */
	bool isValid = !memcmp(wavHeader->riff_header, &cmpdword, sizeof(fr_u32));
	BugAssert(isValid, "Wrong file format");
	if (!isValid) {
		try {
			/* We can get exception at this place, so we must to verify here */
			BugAssert(pMapper->UnmapPointer(sizeof(wav_header), (fr_ptr&)wavHeader), "Segmentation error");
			pMapper->Close();
		} catch (...) {}
		return false;
	}

	/* Convert RIFF header to pcm format struct */
	riff_to_pcm(wavHeader, &fileFormat);
	if (!pMapper->UnmapPointer(sizeof(wav_header), (fr_ptr&)wavHeader)) {
		pMapper->Close();
		return false;
	}

	/* We can't transform 24-bit audio to other format, because we're not DAW or sound editor */
	isValid = (fileFormat.Bits != 24 && fileFormat.Bits != 8);
	BugAssert(isValid, "The 24-bit and 8-bit WAV Files are unsupported");
	if (!isValid) {
		pMapper->Close();
		return false;
	}

	isValid = (!fileFormat.Frames);
	BugAssert(isValid, "There's no samples here");
	if (!isValid) {
		pMapper->Close();
		return false;
	}

	isValid = (!fileFormat.Channels);
	BugAssert(isValid, "There's no channels here");
	if (!isValid) {
		pMapper->Close();
		return false;
	}

	isValid = (!fileFormat.SampleRate);
	BugAssert(isValid, "There's no sample rate here");
	if (!isValid) {
		pMapper->Close();
		return false;
	}

	/* Map this thing */
	if (!pMapper->MapFile(pMappedArea, 0, eMappingRead)) {
		pMapper->Close();
		return false;
	}

	FileFrames = fileFormat.Frames;
	return true;
}

bool
CRIFFMediaResource::CloseResource()
{
	if (pMapper) if (pMapper->UnmapFile(pMappedArea)) pMapper->Close();	
	_RELEASE(pMapper);
	return true;
}

void 
CRIFFMediaResource::GetVendorName(const char*& vendorName)
{
	vendorName = "RIFF";
}

void 
CRIFFMediaResource::GetVendorString(const char*& vendorString)
{
	vendorString = "Simple WAV File";
}

void 
CRIFFMediaResource::GetFormat(PcmFormat& format)
{
	format = fileFormat;	
}

void 
CRIFFMediaResource::SetFormat(PcmFormat outputFormat)
{
	this->outputFormat = outputFormat;
	resampler.Reset(outputFormat.Frames, fileFormat.SampleRate, outputFormat.SampleRate, fileFormat.Channels, false);
}

fr_f32
CRIFFMediaResource::GetSample(fr_i64 Index)
{
	fr_ptr tempMap = fr_ptr((fr_u64)pMappedArea + (fr_u64)(sizeof(wav_header)));
	return fileFormat.IsFloat ? ((fr_f32*)tempMap)[Index] : i16tof32(((fr_i16*)tempMap)[Index]);
}

bool 
CRIFFMediaResource::Read(fr_i64 FramesCount, fr_f32** ppFloatData)
{
	fr_i64 frame_out = 0;

	/* Translate current frames count for output buffer to file format sample rate frames count */
	CalculateFrames64(FramesCount, outputFormat.SampleRate, fileFormat.SampleRate, frame_out);
	fr_i64 FreeFrames = min(frame_out, FileFrames - FramePosition);
	if (!FreeFrames) {		
		/* Set position to 0 for replay */ 
		FramePosition = 0;
		return false;
	}

	if (!ReadRaw(FreeFrames, transferBuffers.GetBuffers())) return false;
	/* Copy data to 64-bit float buffer and resample to output format */
	if (outputFormat.SampleRate != fileFormat.SampleRate) {
		for (size_t i = 0; i < 2; i++) {
			resamplerBuffers[i].Resize(fileFormat.Channels, FreeFrames);
		}

		/* #TODO: Create resampler for 32-float values */
		FloatToDouble(transferBuffers.GetBuffers(), resamplerBuffers[0].GetBuffers(), fileFormat.Channels, (fr_i32)FreeFrames);
		resampler.Resample((fr_i32)FreeFrames, resamplerBuffers[0].GetBuffers(), resamplerBuffers[1].GetBuffers());
		CalculateFrames64(frame_out, fileFormat.SampleRate, outputFormat.SampleRate, frame_out);
		DoubleToFloat(transferBuffers.GetBuffers(), resamplerBuffers[1].GetBuffers(), fileFormat.Channels, (fr_i32)frame_out);
	}

	for (size_t i = 0; i < fileFormat.Channels; i++) {
		memcpy(ppFloatData[i], transferBuffers.GetBufferData(i), frame_out * sizeof(fr_f32));
	}

	return true;
}

bool 
CRIFFMediaResource::ReadRaw(fr_i64 FramesCount, fr_f32** ppFloatData)
{
	tempBuffer.Resize(FramesCount * fileFormat.Channels);
	transferBuffers.Resize(fileFormat.Channels, FramesCount);

	/* Read raw data from mapped buffer by memcpy function */
	try {
		if (fileFormat.IsFloat) {
			fr_u64 to_data = (fr_u64)pMappedArea + sizeof(wav_header);
			to_data += FramePosition * (fileFormat.IsFloat ? sizeof(fr_f32) : sizeof(fr_i16)) * fileFormat.Channels;
			memcpy(tempBuffer.Data(), fr_ptr(to_data), FramesCount * sizeof(fr_f32) * fileFormat.Channels);
		} else {
			/* Try to convert signed 16 to float 32 signal */
			for (size_t i = 0; i < FramesCount * fileFormat.Channels; i++) {
				tempBuffer[i] = this->GetSample(FramePosition * fileFormat.Channels + i);
			}
		}
	} catch (...) {
		return false;
	}
	
	/* Convert interleaved to planar buffer */
	for (size_t i = 0; i < FramesCount * fileFormat.Channels; i++) {
		transferBuffers[i % fileFormat.Channels][i / fileFormat.Channels] = tempBuffer[i];
	}

	FramePosition += FramesCount;
	return true;
}

fr_i64 
CRIFFMediaResource::SetPosition(fr_i64 FramePosition)
{
	fr_i64 frame_out = 0;
	CalculateFrames64(FramePosition, outputFormat.SampleRate, fileFormat.SampleRate, frame_out);
	return (this->FramePosition = FramePosition);
}