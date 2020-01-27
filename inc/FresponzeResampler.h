/*****************************************************************
* Copyright (C) Vertver, 2019. All rights reserved.
* Fresponze - fast, simple and modern multimedia sound library
* Apache-2 License
******************************************************************
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
#pragma once
#include "FresponzeTypes.h"
#include "CDSPResampler.h"

class CR8BrainResampler
{
private:
	r8b::CDSPResampler* resampler[MAX_CHANNELS] = {};
	fr_i32 bufLength = 0;
	fr_i32 inSRate = 0;
	fr_i32 outSRate = 0;
	fr_i32 channels = 0;

public:
	void Initialize(fr_i32 MaxBufferIn, fr_i32 InputSampleRate, fr_i32 OutputSampleRate, fr_i32 ChannelsCount, bool isLinear)
	{
		bufLength = MaxBufferIn;
		inSRate = InputSampleRate;
		outSRate = OutputSampleRate;
		channels = ChannelsCount;
		for (size_t i = 0; i < ChannelsCount; i++){
			resampler[i] = new r8b::CDSPResampler(inSRate, outSRate, MaxBufferIn, 2.0, 206.91, isLinear ? r8b::fprLinearPhase : r8b::fprMinPhase);
		}	
	}

	void Destroy()
	{
		size_t index = 0;
		while (!!resampler[index]) {
			delete resampler[index];
		}
	}

	void Reset(fr_i32 MaxBufferIn, fr_i32 InputSampleRate, fr_i32 OutputSampleRate, fr_i32 ChannelsCount, bool isLinear)
	{
		Destroy();
		Initialize(MaxBufferIn, InputSampleRate, OutputSampleRate, ChannelsCount, isLinear);
	}
	
	void Resample(fr_f64** inputData, fr_f64** outputData)
	{
		for (size_t i = 0; i < channels; i++) {
			resampler[i]->process(inputData[i], bufLength, outputData[i]);
		}
	}
};