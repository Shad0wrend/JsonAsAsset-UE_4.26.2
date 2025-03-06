// Copyright JAA Contributors 2024-2025

#pragma once

#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

class FRemoteUtilities {
public:
#if ENGINE_MAJOR_VERSION >= 5
	static TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> ExecuteRequestSync(TSharedRef<IHttpRequest> HttpRequest, float LoopDelay = 0.1);
#else
	static TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> ExecuteRequestSync(const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& HttpRequest, float LoopDelay = 0.1);
#endif
};
