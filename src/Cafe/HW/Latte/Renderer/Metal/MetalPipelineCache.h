#pragma once

#include "Cafe/HW/Latte/Renderer/Metal/MetalPipelineCompiler.h"
#include "util/helpers/ConcurrentQueue.h"
#include "util/helpers/fspinlock.h"
#include "util/math/vector2.h"

class MetalPipelineCache
{
public:
	static MetalPipelineCache& GetInstance();

    MetalPipelineCache(class MetalRenderer* metalRenderer);
    ~MetalPipelineCache();

    PipelineObject* GetRenderPipelineState(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, const class MetalAttachmentsInfo& lastUsedAttachmentsInfo, const class MetalAttachmentsInfo& activeAttachmentsInfo, Vector2i extend, uint32 indexCount, const LatteContextRegister& lcr);

    // Cache loading
	uint32 BeginLoading(uint64 cacheTitleId); // returns count of pipelines stored in cache
	bool UpdateLoading(uint32& pipelinesLoadedTotal, uint32& pipelinesMissingShaders);
	void EndLoading();
	void LoadPipelineFromCache(std::span<uint8> fileData);
       void Close(); // called on title exit

    // Debug
    size_t GetPipelineCacheSize() const { return m_pipelineCache.size(); }

    // nullptr when no title is loaded, persistence is off, or archive creation failed
    // this session - see m_binaryArchive's own comment below for why that is fine.
    MTL::BinaryArchive* GetBinaryArchive() const { return m_binaryArchive; }

private:
    class MetalRenderer* m_mtlr;

    std::map<uint64, PipelineObject*> m_pipelineCache;
    FSpinlock m_pipelineCacheLock;

	std::thread* m_pipelineCacheStoreThread;

	class FileCache* s_cache;

	// Precompiled Metal binary cache. Separate from s_cache above: s_cache stores just
	// enough (shader hashes + fixed-function state) to know WHICH pipelines to eagerly
	// rebuild at boot, but MetalPipelineCompiler::Compile() still calls the real
	// newRenderPipelineState() fresh every launch - the actual machine-code compile is
	// not what s_cache skips. MTL::BinaryArchive is Apple's on-device mechanism for
	// that: attach it to a pipeline descriptor before compiling, and Metal serves a
	// matching entry from the archive instead of recompiling. nullptr when unavailable
	// (creation failed, or persistence is off) - every caller must tolerate that and
	// fall back to a normal (uncached) compile.
	MTL::BinaryArchive* m_binaryArchive = nullptr;
	// Set alongside m_binaryArchive in BeginLoading(), read back in Close() to rebuild
	// the same path for serializeToURL() - Close() itself is called with no arguments
	// from LatteShaderCache_Close(), so this is simpler than changing that signature
	// for every renderer backend's own Close().
	uint64 m_binaryArchiveTitleId = 0;

	std::atomic_uint32_t m_numCompilationThreads{ 0 };
	ConcurrentQueue<std::vector<uint8>> m_compilationQueue;
	std::atomic_uint32_t m_compilationCount;

    static uint64 CalculatePipelineHash(const LatteFetchShader* fetchShader, const LatteDecompilerShader* vertexShader, const LatteDecompilerShader* geometryShader, const LatteDecompilerShader* pixelShader, const class MetalAttachmentsInfo& lastUsedAttachmentsInfo, const class MetalAttachmentsInfo& activeAttachmentsInfo, const LatteContextRegister& lcr);

    void AddCurrentStateToCache(uint64 pipelineStateHash, const class MetalAttachmentsInfo& lastUsedAttachmentsInfo);

	// pipeline serialization for file
	bool SerializePipeline(class MemStreamWriter& memWriter, struct CachedPipeline& cachedPipeline);
	bool DeserializePipeline(class MemStreamReader& memReader, struct CachedPipeline& cachedPipeline);

    int CompilerThread();
	void WorkerThread();
};
