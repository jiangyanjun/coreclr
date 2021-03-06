// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#include "common.h"
#include "clrtypes.h"
#include "safemath.h"
#include "eventpipe.h"
#include "eventpipebuffermanager.h"
#include "eventpipeconfiguration.h"
#include "eventpipeevent.h"
#include "eventpipeeventsource.h"
#include "eventpipefile.h"
#include "eventpipeprovider.h"
#include "eventpipesession.h"
#include "eventpipejsonfile.h"
#include "eventtracebase.h"
#include "sampleprofiler.h"

#ifdef FEATURE_PAL
#include "pal.h"
#endif // FEATURE_PAL

#ifdef FEATURE_PERFTRACING

CrstStatic EventPipe::s_configCrst;
bool EventPipe::s_tracingInitialized = false;
EventPipeConfiguration* EventPipe::s_pConfig = NULL;
EventPipeSession* EventPipe::s_pSession = NULL;
EventPipeBufferManager* EventPipe::s_pBufferManager = NULL;
EventPipeFile* EventPipe::s_pFile = NULL;
EventPipeEventSource* EventPipe::s_pEventSource = NULL;
LPCWSTR EventPipe::s_pCommandLine = NULL;
#ifdef _DEBUG
EventPipeFile* EventPipe::s_pSyncFile = NULL;
EventPipeJsonFile* EventPipe::s_pJsonFile = NULL;
#endif // _DEBUG

#ifdef FEATURE_PAL
// This function is auto-generated from /src/scripts/genEventPipe.py
extern "C" void InitProvidersAndEvents();
#else
void InitProvidersAndEvents();
#endif

EventPipeEventPayload::EventPipeEventPayload(BYTE *pData, unsigned int length)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    m_pData = pData;
    m_pEventData = NULL;
    m_eventDataCount = 0;
    m_allocatedData = false;

    m_size = length;
}

EventPipeEventPayload::EventPipeEventPayload(EventData *pEventData, unsigned int eventDataCount)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    m_pData = NULL;
    m_pEventData = pEventData;
    m_eventDataCount = eventDataCount;
    m_allocatedData = false;

    S_UINT32 tmp_size = S_UINT32(0);
    for (unsigned int i=0; i<m_eventDataCount; i++)
    {
        tmp_size += S_UINT32(m_pEventData[i].Size);
    }

    if (tmp_size.IsOverflow())
    {
        // If there is an overflow, drop the data and create an empty payload
        m_pEventData = NULL;
        m_eventDataCount = 0;
        m_size = 0;
    }
    else
    {
        m_size = tmp_size.Value();
    }
}

EventPipeEventPayload::~EventPipeEventPayload()
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    if(m_allocatedData && m_pData != NULL)
    {
        delete[] m_pData;
        m_pData = NULL;
    }
}

void EventPipeEventPayload::Flatten()
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    if(m_size > 0)
    {
        if (!IsFlattened())
        {
            BYTE* tmp_pData = new (nothrow) BYTE[m_size];
            if (tmp_pData != NULL)
            {
                m_allocatedData = true;
                CopyData(tmp_pData);
                m_pData = tmp_pData;
            }
        }
    }
}

void EventPipeEventPayload::CopyData(BYTE *pDst)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    if(m_size > 0)
    {
        if(IsFlattened())
        {
            memcpy(pDst, m_pData, m_size);
        }

        else if(m_pEventData != NULL)
        {
            unsigned int offset = 0;
            for(unsigned int i=0; i<m_eventDataCount; i++)
            {
                memcpy(pDst + offset, (BYTE*) m_pEventData[i].Ptr, m_pEventData[i].Size);
                offset += m_pEventData[i].Size;
            }
        }
    }
}

BYTE* EventPipeEventPayload::GetFlatData()
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    if (!IsFlattened())
    {
        Flatten();
    }
    return m_pData;
}

void EventPipe::Initialize()
{
    STANDARD_VM_CONTRACT;

    s_tracingInitialized = s_configCrst.InitNoThrow(
        CrstEventPipe,
        (CrstFlags)(CRST_REENTRANCY | CRST_TAKEN_DURING_SHUTDOWN | CRST_HOST_BREAKABLE));

    s_pConfig = new EventPipeConfiguration();
    s_pConfig->Initialize();

    s_pBufferManager = new EventPipeBufferManager();

    s_pEventSource = new EventPipeEventSource();

    // This calls into auto-generated code to initialize the runtime providers
    // and events so that the EventPipe configuration lock isn't taken at runtime
    InitProvidersAndEvents();
}

void EventPipe::EnableOnStartup()
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_ANY;
    }
    CONTRACTL_END;

    // Test COMPLUS variable to enable tracing at start-up.
    if((CLRConfig::GetConfigValue(CLRConfig::INTERNAL_EnableEventPipe) & 1) == 1)
    {
        SString outputPath;
        outputPath.Printf("Process-%d.netperf", GetCurrentProcessId());

        // Create a new session.
        EventPipeSession *pSession = new EventPipeSession(
            1024 /* 1 GB circular buffer */,
            NULL, /* pProviders */
            0 /* numProviders */);

        // Get the configuration from the environment.
        GetConfigurationFromEnvironment(outputPath, pSession);

        // Enable the session.
        Enable(outputPath, pSession);
    }
}

void EventPipe::Shutdown()
{
    CONTRACTL
    {
        NOTHROW;
        GC_TRIGGERS;
        MODE_ANY;
    }
    CONTRACTL_END;

    // Mark tracing as no longer initialized.
    s_tracingInitialized = false;

    // We are shutting down, so if disabling EventPipe throws, we need to move along anyway.
    EX_TRY
    {
        Disable();
    }
    EX_CATCH { }
    EX_END_CATCH(SwallowAllExceptions);

    // Save pointers to the configuration and buffer manager.
    EventPipeConfiguration *pConfig = s_pConfig;
    EventPipeBufferManager *pBufferManager = s_pBufferManager;

    // Set the static pointers to NULL so that the rest of the EventPipe knows that they are no longer available.
    // Flush process write buffers to make sure other threads can see the change.
    s_pConfig = NULL;
    s_pBufferManager = NULL;
    FlushProcessWriteBuffers();

    // Free resources.
    delete(pConfig);
    delete(pBufferManager);
    delete(s_pEventSource);
    s_pEventSource = NULL;

    // On Windows, this is just a pointer to the return value from
    // GetCommandLineW(), so don't attempt to free it.
#ifdef FEATURE_PAL
    delete[](s_pCommandLine);
    s_pCommandLine = NULL;
#endif
}

void EventPipe::Enable(
    LPCWSTR strOutputPath,
    unsigned int circularBufferSizeInMB,
    EventPipeProviderConfiguration *pProviders,
    int numProviders)
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_ANY;
    }
    CONTRACTL_END;

    // Create a new session.
    EventPipeSession *pSession = s_pConfig->CreateSession(circularBufferSizeInMB, pProviders, static_cast<unsigned int>(numProviders));

    // Enable the session.
    Enable(strOutputPath, pSession);
}

void EventPipe::Enable(LPCWSTR strOutputPath, EventPipeSession *pSession)
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_ANY;
        PRECONDITION(pSession != NULL);
    }
    CONTRACTL_END;

    // If tracing is not initialized or is already enabled, bail here.
    if(!s_tracingInitialized || s_pConfig == NULL || s_pConfig->Enabled())
    {
        return;
    }

    // If the state or arguments are invalid, bail here.
    if(pSession == NULL || !pSession->IsValid())
    {
        return;
    }

    // Enable the EventPipe EventSource.
    s_pEventSource->Enable(pSession);

    // Take the lock before enabling tracing.
    CrstHolder _crst(GetLock());

    // Create the event pipe file.
    SString eventPipeFileOutputPath(strOutputPath);
    s_pFile = new EventPipeFile(eventPipeFileOutputPath);

#ifdef _DEBUG
    if((CLRConfig::GetConfigValue(CLRConfig::INTERNAL_EnableEventPipe) & 2) == 2)
    {
        // Create a synchronous file.
        SString eventPipeSyncFileOutputPath;
        eventPipeSyncFileOutputPath.Printf("Process-%d.sync.netperf", GetCurrentProcessId());
        s_pSyncFile = new EventPipeFile(eventPipeSyncFileOutputPath);

        // Create a JSON file.
        SString outputFilePath;
        outputFilePath.Printf("Process-%d.PerfView.json", GetCurrentProcessId());
        s_pJsonFile = new EventPipeJsonFile(outputFilePath);
    }
#endif // _DEBUG

    // Save the session.
    s_pSession = pSession;

    // Enable tracing.
    s_pConfig->Enable(s_pSession);

    // Enable the sample profiler
    SampleProfiler::Enable();
}

void EventPipe::Disable()
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_ANY;
    }
    CONTRACTL_END;

    // Don't block GC during clean-up.
    GCX_PREEMP();

    // Take the lock before disabling tracing.
    CrstHolder _crst(GetLock());

    if(s_pConfig != NULL && s_pConfig->Enabled())
    {
        // Disable the profiler.
        SampleProfiler::Disable();

        // Log the process information event.
        s_pEventSource->SendProcessInfo(s_pCommandLine);

        // Log the runtime information event.
        ETW::InfoLog::RuntimeInformation(ETW::InfoLog::InfoStructs::Normal);

        // Disable tracing.
        s_pConfig->Disable(s_pSession);

        // Delete the session.
        s_pConfig->DeleteSession(s_pSession);
        s_pSession = NULL;

        // Flush all write buffers to make sure that all threads see the change.
        FlushProcessWriteBuffers();

        // Write to the file.
        LARGE_INTEGER disableTimeStamp;
        QueryPerformanceCounter(&disableTimeStamp);
        s_pBufferManager->WriteAllBuffersToFile(s_pFile, disableTimeStamp);

        if(CLRConfig::GetConfigValue(CLRConfig::INTERNAL_EventPipeRundown) > 0)
        {
            // Before closing the file, do rundown.
            const unsigned int numRundownProviders = 2;
            EventPipeProviderConfiguration rundownProviders[] =
            {
                { W("Microsoft-Windows-DotNETRuntime"), 0x80020138, static_cast<unsigned int>(EventPipeEventLevel::Verbose) }, // Public provider.
                { W("Microsoft-Windows-DotNETRuntimeRundown"), 0x80020138, static_cast<unsigned int>(EventPipeEventLevel::Verbose) } // Rundown provider.
            };
            // The circular buffer size doesn't matter because all events are written synchronously during rundown.
            s_pSession = s_pConfig->CreateSession(1 /* circularBufferSizeInMB */, rundownProviders, numRundownProviders);
            s_pConfig->EnableRundown(s_pSession);

            // Ask the runtime to emit rundown events.
            if(g_fEEStarted && !g_fEEShutDown)
            {
                ETW::EnumerationLog::EndRundown();
            }

            // Disable the event pipe now that rundown is complete.
            s_pConfig->Disable(s_pSession);

            // Delete the rundown session.
            s_pConfig->DeleteSession(s_pSession);
            s_pSession = NULL;
        }

        if(s_pFile != NULL)
        {
            delete(s_pFile);
            s_pFile = NULL;
        }
#ifdef _DEBUG
        if(s_pSyncFile != NULL)
        {
            delete(s_pSyncFile);
            s_pSyncFile = NULL;
        }
        if(s_pJsonFile != NULL)
        {
            delete(s_pJsonFile);
            s_pJsonFile = NULL;
        }
#endif // _DEBUG

        // De-allocate buffers.
        s_pBufferManager->DeAllocateBuffers();

        // Delete deferred providers.
        // Providers can't be deleted during tracing because they may be needed when serializing the file.
        s_pConfig->DeleteDeferredProviders();
    }
}

bool EventPipe::Enabled()
{
    LIMITED_METHOD_CONTRACT;

    bool enabled = false;
    if(s_pConfig != NULL)
    {
        enabled = s_pConfig->Enabled();
    }

    return enabled;
}

EventPipeProvider* EventPipe::CreateProvider(const SString &providerName, EventPipeCallback pCallbackFunction, void *pCallbackData)
{
    CONTRACTL
    {
        THROWS;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    EventPipeProvider *pProvider = NULL;
    if (s_pConfig != NULL)
    {
        pProvider = s_pConfig->CreateProvider(providerName, pCallbackFunction, pCallbackData);
    }

    return pProvider;

}

void EventPipe::DeleteProvider(EventPipeProvider *pProvider)
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_ANY;
    }
    CONTRACTL_END;

    // Take the lock to make sure that we don't have a race
    // between disabling tracing and deleting a provider
    // where we hold a provider after tracing has been disabled.
    CrstHolder _crst(GetLock());

    if(pProvider != NULL)
    {
        if(Enabled())
        {
            // Save the provider until the end of the tracing session.
            pProvider->SetDeleteDeferred();
        }
        else
        {
            // Delete the provider now.
            if (s_pConfig != NULL)
            {
                s_pConfig->DeleteProvider(pProvider);
            }
        }
    }
}

void EventPipe::WriteEvent(EventPipeEvent &event, BYTE *pData, unsigned int length, LPCGUID pActivityId, LPCGUID pRelatedActivityId)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    EventPipeEventPayload payload(pData, length);
    EventPipe::WriteEventInternal(event, payload, pActivityId, pRelatedActivityId);
}

void EventPipe::WriteEvent(EventPipeEvent &event, EventData *pEventData, unsigned int eventDataCount, LPCGUID pActivityId, LPCGUID pRelatedActivityId)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    EventPipeEventPayload payload(pEventData, eventDataCount);
    EventPipe::WriteEventInternal(event, payload, pActivityId, pRelatedActivityId);
}

void EventPipe::WriteEventInternal(EventPipeEvent &event, EventPipeEventPayload &payload, LPCGUID pActivityId, LPCGUID pRelatedActivityId)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
        PRECONDITION(s_pSession != NULL);
    }
    CONTRACTL_END;

    // Exit early if the event is not enabled.
    if(!event.IsEnabled())
    {
        return;
    }

    // Get the current thread;
    Thread *pThread = GetThread();
    if(pThread == NULL)
    {
        // We can't write an event without the thread object.
        return;
    }

    if(s_pConfig == NULL)
    {
        // We can't procede without a configuration
        return;
    }

    // If the activity id isn't specified, pull it from the current thread.
    if(pActivityId == NULL)
    {
        pActivityId = pThread->GetActivityId();
    }

    if(!s_pConfig->RundownEnabled() && s_pBufferManager != NULL)
    {
        if(!s_pBufferManager->WriteEvent(pThread, *s_pSession, event, payload, pActivityId, pRelatedActivityId))
        {
            // This is used in DEBUG to make sure that we don't log an event synchronously that we didn't log to the buffer.
            return;
        }
    }
    else if(s_pConfig->RundownEnabled())
    {
        // It is possible that some events that are enabled on rundown can be emitted from other threads.
        // We're not interested in these events and they can cause corrupted trace files because rundown
        // events are written synchronously and not under lock.
        // If we encounter an event that did not originate on the thread that is doing rundown, ignore it.
        if(!s_pConfig->IsRundownThread(pThread))
        {
            return;
        }

        BYTE *pData = payload.GetFlatData();
        if (pData != NULL)
        {
            // Write synchronously to the file.
            // We're under lock and blocking the disabling thread.
            // This copy occurs here (rather than at file write) because
            // A) The FastSerializer API would need to change if we waited
            // B) It is unclear there is a benefit to multiple file write calls
            //    as opposed a a buffer copy here
            EventPipeEventInstance instance(
                *s_pSession,
                event,
                pThread->GetOSThreadId(),
                pData,
                payload.GetSize(),
                pActivityId,
                pRelatedActivityId);

            if(s_pFile != NULL)
            {
                // EventPipeFile::WriteEvent needs to allocate a metadata event
                // and can therefore throw. In this context we will silently
                // fail rather than disrupt the caller
                EX_TRY
                {
                    s_pFile->WriteEvent(instance);
                }
                EX_CATCH { }
                EX_END_CATCH(SwallowAllExceptions);
            }
        }
    }

// This section requires a call to GCX_PREEMP which violates the GC_NOTRIGGER contract
// It should only be enabled when debugging this specific component and contracts are off
#ifdef DEBUG_JSON_EVENT_FILE
    {
        GCX_PREEMP();

        BYTE *pData = payload.GetFlatData();
        if (pData != NULL)
        {
            // Create an instance of the event for the synchronous path.
            EventPipeEventInstance instance(
                event,
                pThread->GetOSThreadId(),
                pData,
                payload.GetSize(),
                pActivityId,
                pRelatedActivityId);

            // Write to the EventPipeFile if it exists.
            if(s_pSyncFile != NULL)
            {
                s_pSyncFile->WriteEvent(instance);
            }

            // Write to the EventPipeJsonFile if it exists.
            if(s_pJsonFile != NULL)
            {
                s_pJsonFile->WriteEvent(instance);
            }
        }
    }
#endif // DEBUG_JSON_EVENT_FILE
}

void EventPipe::WriteSampleProfileEvent(Thread *pSamplingThread, EventPipeEvent *pEvent, Thread *pTargetThread, StackContents &stackContents, BYTE *pData, unsigned int length)
{
    CONTRACTL
    {
        NOTHROW;
        GC_TRIGGERS;
        MODE_PREEMPTIVE;
    }
    CONTRACTL_END;

    EventPipeEventPayload payload(pData, length);

    // Write the event to the thread's buffer.
    if(s_pBufferManager != NULL)
    {
        // Specify the sampling thread as the "current thread", so that we select the right buffer.
        // Specify the target thread so that the event gets properly attributed.
        if(!s_pBufferManager->WriteEvent(pSamplingThread, *s_pSession, *pEvent, payload, NULL /* pActivityId */, NULL /* pRelatedActivityId */, pTargetThread, &stackContents))
        {
            // This is used in DEBUG to make sure that we don't log an event synchronously that we didn't log to the buffer.
            return;
        }
    }

#ifdef _DEBUG
    {
        GCX_PREEMP();

        // Create an instance for the synchronous path.
        SampleProfilerEventInstance instance(*s_pSession, *pEvent, pTargetThread, pData, length);
        stackContents.CopyTo(instance.GetStack());

        // Write to the EventPipeFile.
        if(s_pSyncFile != NULL)
        {
            s_pSyncFile->WriteEvent(instance);
        }

        // Write to the EventPipeJsonFile if it exists.
        if(s_pJsonFile != NULL)
        {
            s_pJsonFile->WriteEvent(instance);
        }
    }
#endif // _DEBUG
}

bool EventPipe::WalkManagedStackForCurrentThread(StackContents &stackContents)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;

    Thread *pThread = GetThread();
    if(pThread != NULL)
    {
        return WalkManagedStackForThread(pThread, stackContents);
    }

    return false;
}

bool EventPipe::WalkManagedStackForThread(Thread *pThread, StackContents &stackContents)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
        PRECONDITION(pThread != NULL);
    }
    CONTRACTL_END;

    // Calling into StackWalkFrames in preemptive mode violates the host contract,
    // but this contract is not used on CoreCLR.
    CONTRACT_VIOLATION( HostViolation );

    stackContents.Reset();

    StackWalkAction swaRet = pThread->StackWalkFrames(
        (PSTACKWALKFRAMESCALLBACK) &StackWalkCallback,
        &stackContents,
        ALLOW_ASYNC_STACK_WALK | FUNCTIONSONLY | HANDLESKIPPEDFRAMES);

    return ((swaRet == SWA_DONE) || (swaRet == SWA_CONTINUE));
}

StackWalkAction EventPipe::StackWalkCallback(CrawlFrame *pCf, StackContents *pData)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
        PRECONDITION(pCf != NULL);
        PRECONDITION(pData != NULL);
    }
    CONTRACTL_END;

    // Get the IP.
    UINT_PTR controlPC = (UINT_PTR)pCf->GetRegisterSet()->ControlPC;
    if(controlPC == 0)
    {
        if(pData->GetLength() == 0)
        {
            // This happens for pinvoke stubs on the top of the stack.
            return SWA_CONTINUE;
        }
    }

    _ASSERTE(controlPC != 0);

    // Add the IP to the captured stack.
    pData->Append(
        controlPC,
        pCf->GetFunction()
        );

    // Continue the stack walk.
    return SWA_CONTINUE;
}

EventPipeConfiguration* EventPipe::GetConfiguration()
{
    LIMITED_METHOD_CONTRACT;

    return s_pConfig;
}

CrstStatic* EventPipe::GetLock()
{
    LIMITED_METHOD_CONTRACT;

    return &s_configCrst;
}

void EventPipe::GetConfigurationFromEnvironment(SString &outputPath, EventPipeSession *pSession)
{
    LIMITED_METHOD_CONTRACT;

    // Set the output path if specified.
    CLRConfigStringHolder wszOutputPath(CLRConfig::GetConfigValue(CLRConfig::INTERNAL_EventPipeOutputFile));
    if(wszOutputPath != NULL)
    {
        outputPath.Set(wszOutputPath);
    }

    // Read the the provider configuration from the environment if specified.
    CLRConfigStringHolder wszConfig(CLRConfig::GetConfigValue(CLRConfig::INTERNAL_EventPipeConfig));
    if(wszConfig == NULL)
    {
        pSession->EnableAllEvents();
        return;
    }

    size_t len = wcslen(wszConfig);
    if(len <= 0)
    {
        pSession->EnableAllEvents();
        return;
    }

    // Parses a string with the following format:
    //
    //      ProviderName:Keywords:Level[,]*
    //
    // For example:
    //
    //      Microsoft-Windows-DotNETRuntime:0xCAFEBABE:2,Microsoft-Windows-DotNETRuntimePrivate:0xDEADBEEF:1
    //
    // Each provider configuration is separated by a ',' and each component within the configuration is
    // separated by a ':'.

    const WCHAR ProviderSeparatorChar = ',';
    const WCHAR ComponentSeparatorChar = ':';
    size_t index = 0;
    WCHAR *pProviderName = NULL;
    UINT64 keywords = 0;
    EventPipeEventLevel level = EventPipeEventLevel::Critical;

    while(index < len)
    {
        WCHAR * pCurrentChunk = &wszConfig[index];
        size_t currentChunkStartIndex = index;
        size_t currentChunkEndIndex = 0;

        // Find the next chunk.
        while(index < len && wszConfig[index] != ProviderSeparatorChar)
        {
            index++;
        }
        currentChunkEndIndex = index++;

        // Split the chunk into components.
        size_t chunkIndex = currentChunkStartIndex;

        // Get the provider name.
        size_t provNameStartIndex = chunkIndex;
        size_t provNameEndIndex = currentChunkEndIndex;

        while(chunkIndex < currentChunkEndIndex && wszConfig[chunkIndex] != ComponentSeparatorChar)
        {
            chunkIndex++;
        }
        provNameEndIndex = chunkIndex++;

        size_t provNameLen = provNameEndIndex - provNameStartIndex;
        pProviderName = new WCHAR[provNameLen+1];
        memcpy(pProviderName, &wszConfig[provNameStartIndex], provNameLen*sizeof(WCHAR));
        pProviderName[provNameLen] = '\0';

        // Get the keywords.
        size_t keywordsStartIndex = chunkIndex;
        size_t keywordsEndIndex = currentChunkEndIndex;

        while(chunkIndex < currentChunkEndIndex && wszConfig[chunkIndex] != ComponentSeparatorChar)
        {
            chunkIndex++;
        }
        keywordsEndIndex = chunkIndex++;

        size_t keywordsLen = keywordsEndIndex - keywordsStartIndex;
        WCHAR *wszKeywords = new WCHAR[keywordsLen+1];
        memcpy(wszKeywords, &wszConfig[keywordsStartIndex], keywordsLen*sizeof(WCHAR));
        wszKeywords[keywordsLen] = '\0';
        keywords = _wcstoui64(wszKeywords, NULL, 16);
        delete[] wszKeywords;
        wszKeywords = NULL;

        // Get the level.
        size_t levelStartIndex = chunkIndex;
        size_t levelEndIndex = currentChunkEndIndex;

        while(chunkIndex < currentChunkEndIndex && wszConfig[chunkIndex] != ComponentSeparatorChar)
        {
            chunkIndex++;
        }
        levelEndIndex = chunkIndex++;

        size_t levelLen = levelEndIndex - levelStartIndex;
        WCHAR *wszLevel = new WCHAR[levelLen+1];
        memcpy(wszLevel, &wszConfig[levelStartIndex], levelLen*sizeof(WCHAR));
        wszLevel[levelLen] = '\0';
        level = (EventPipeEventLevel) wcstoul(wszLevel, NULL, 16);
        delete[] wszLevel;
        wszLevel = NULL;

        // Add a new EventPipeSessionProvider.
        EventPipeSessionProvider *pSessionProvider = new EventPipeSessionProvider(pProviderName, keywords, level);
        pSession->AddSessionProvider(pSessionProvider);

        // Free the provider name string.
        if(pProviderName != NULL)
        {
            delete[] pProviderName;
            pProviderName = NULL;
        }
    }
}

void EventPipe::SaveCommandLine(LPCWSTR pwzAssemblyPath, int argc, LPCWSTR *argv)
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_COOPERATIVE;
        PRECONDITION(pwzAssemblyPath != NULL);
        PRECONDITION(argc <= 0 || argv != NULL);
    }
    CONTRACTL_END;

    // Get the command line.
    LPCWSTR osCommandLine = GetCommandLineW();

#ifndef FEATURE_PAL
    // On Windows, osCommandLine contains the executable and all arguments.
    s_pCommandLine = osCommandLine;
#else
    // On UNIX, the PAL doesn't have the command line arguments, so we must build the command line.
    // osCommandLine contains the full path to the executable.
    SString commandLine(osCommandLine);
    commandLine.Append((WCHAR)' ');
    commandLine.Append(pwzAssemblyPath);

    for(int i=0; i<argc; i++)
    {
        commandLine.Append((WCHAR)' ');
        commandLine.Append(argv[i]);
    }

    // Allocate a new string for the command line.
    SIZE_T commandLineLen = commandLine.GetCount();
    WCHAR *pCommandLine = new WCHAR[commandLineLen + 1];
    wcsncpy(pCommandLine, commandLine.GetUnicode(), commandLineLen);
    pCommandLine[commandLineLen] = '\0';

    s_pCommandLine = pCommandLine;
#endif
}

void QCALLTYPE EventPipeInternal::Enable(
        __in_z LPCWSTR outputFile,
        UINT32 circularBufferSizeInMB,
        INT64 profilerSamplingRateInNanoseconds,
        EventPipeProviderConfiguration *pProviders,
        INT32 numProviders)
{
    QCALL_CONTRACT;

    BEGIN_QCALL;
    SampleProfiler::SetSamplingRate((unsigned long)profilerSamplingRateInNanoseconds);
    EventPipe::Enable(outputFile, circularBufferSizeInMB, pProviders, numProviders);
    END_QCALL;
}

void QCALLTYPE EventPipeInternal::Disable()
{
    QCALL_CONTRACT;

    BEGIN_QCALL;
    EventPipe::Disable();
    END_QCALL;
}

INT_PTR QCALLTYPE EventPipeInternal::CreateProvider(
    __in_z LPCWSTR providerName,
    EventPipeCallback pCallbackFunc)
{
    QCALL_CONTRACT;

    EventPipeProvider *pProvider = NULL;

    BEGIN_QCALL;

    pProvider = EventPipe::CreateProvider(providerName, pCallbackFunc, NULL);

    END_QCALL;

    return reinterpret_cast<INT_PTR>(pProvider);
}

INT_PTR QCALLTYPE EventPipeInternal::DefineEvent(
    INT_PTR provHandle,
    UINT32 eventID,
    __int64 keywords,
    UINT32 eventVersion,
    UINT32 level,
    void *pMetadata,
    UINT32 metadataLength)
{
    QCALL_CONTRACT;

    EventPipeEvent *pEvent = NULL;

    BEGIN_QCALL;

    _ASSERTE(provHandle != NULL);
    EventPipeProvider *pProvider = reinterpret_cast<EventPipeProvider *>(provHandle);
    pEvent = pProvider->AddEvent(eventID, keywords, eventVersion, (EventPipeEventLevel)level, (BYTE *)pMetadata, metadataLength);
    _ASSERTE(pEvent != NULL);

    END_QCALL;

    return reinterpret_cast<INT_PTR>(pEvent);
}

void QCALLTYPE EventPipeInternal::DeleteProvider(
    INT_PTR provHandle)
{
    QCALL_CONTRACT;
    BEGIN_QCALL;

    if(provHandle != NULL)
    {
        EventPipeProvider *pProvider = reinterpret_cast<EventPipeProvider*>(provHandle);
        EventPipe::DeleteProvider(pProvider);
    }

    END_QCALL;
}

int QCALLTYPE EventPipeInternal::EventActivityIdControl(
    uint controlCode,
    GUID *pActivityId)
{

    QCALL_CONTRACT;

    int retVal = 0;

    BEGIN_QCALL;

    Thread *pThread = GetThread();
    if(pThread == NULL || pActivityId == NULL)
    {
        retVal = 1;
    }
    else
    {
        ActivityControlCode activityControlCode = (ActivityControlCode)controlCode;
        GUID currentActivityId;
        switch(activityControlCode)
        {
            case ActivityControlCode::EVENT_ACTIVITY_CONTROL_GET_ID:

                *pActivityId = *pThread->GetActivityId();
                break;

            case ActivityControlCode::EVENT_ACTIVITY_CONTROL_SET_ID:

                pThread->SetActivityId(pActivityId);
                break;

            case ActivityControlCode::EVENT_ACTIVITY_CONTROL_CREATE_ID:

                CoCreateGuid(pActivityId);
                break;

            case ActivityControlCode::EVENT_ACTIVITY_CONTROL_GET_SET_ID:

                currentActivityId = *pThread->GetActivityId();
                pThread->SetActivityId(pActivityId);
                *pActivityId = currentActivityId;

                break;

            case ActivityControlCode::EVENT_ACTIVITY_CONTROL_CREATE_SET_ID:

                *pActivityId = *pThread->GetActivityId();
                CoCreateGuid(&currentActivityId);
                pThread->SetActivityId(&currentActivityId);
                break;

            default:
                retVal = 1;
        };
    }

    END_QCALL;
    return retVal;
}

void QCALLTYPE EventPipeInternal::WriteEvent(
    INT_PTR eventHandle,
    UINT32 eventID,
    void *pData,
    UINT32 length,
    LPCGUID pActivityId,
    LPCGUID pRelatedActivityId)
{
    QCALL_CONTRACT;
    BEGIN_QCALL;

    _ASSERTE(eventHandle != NULL);
    EventPipeEvent *pEvent = reinterpret_cast<EventPipeEvent *>(eventHandle);
    EventPipe::WriteEvent(*pEvent, (BYTE *)pData, length, pActivityId, pRelatedActivityId);

    END_QCALL;
}

void QCALLTYPE EventPipeInternal::WriteEventData(
    INT_PTR eventHandle,
    UINT32 eventID,
    EventData *pEventData,
    UINT32 eventDataCount,
    LPCGUID pActivityId,
    LPCGUID pRelatedActivityId)
{
    QCALL_CONTRACT;
    BEGIN_QCALL;

    _ASSERTE(eventHandle != NULL);
    EventPipeEvent *pEvent = reinterpret_cast<EventPipeEvent *>(eventHandle);
    EventPipe::WriteEvent(*pEvent, pEventData, eventDataCount, pActivityId, pRelatedActivityId);

    END_QCALL;
}

#endif // FEATURE_PERFTRACING
