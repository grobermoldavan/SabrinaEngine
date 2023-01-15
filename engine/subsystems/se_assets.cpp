
#include "se_assets.hpp"
#include "engine/engine.hpp"

using SeMinimizeAssetPfn = void (*)(void*);
using SeMaximizeAssetPfn = void (*)(void*);
using SeUnloadAssetPfn = void (*)(void*);

struct SeAssetEntry
{
    uint32_t            cpuUsageMin;
    uint32_t            gpuUsageMin;
    uint32_t            cpuUsageMax;
    uint32_t            gpuUsageMax;
    void*               data;
    SeMinimizeAssetPfn  minimize;
    SeMaximizeAssetPfn  maximize;
    SeUnloadAssetPfn    unload;

    SeAssetEntry*       queueNext;
    SeAssetEntry*       queuePrev;
};

struct SeAssetManager
{
    static constexpr const size_t CAPACITY = 1024;
    static constexpr const size_t LEDGER_ENTRY_SIZE_BITS = 64;
    static constexpr const size_t LEDGER_CAPACITY = CAPACITY / LEDGER_ENTRY_SIZE_BITS;

    SeAssetEntry        entries[CAPACITY];
    uint64_t            ledger[LEDGER_CAPACITY];

    size_t              maxCpuUsage;
    size_t              maxGpuUsage;
    size_t              currentCpuUsage;
    size_t              currentGpuUsage;

    SeAssetEntry*       maximizedAssetsQueue;
} g_assetManager = { };

namespace assets
{
    namespace impl
    {
        struct { size_t index; SeAssetEntry* entry; } add_entry()
        {
            size_t resultIndex = SIZE_MAX;
            for (size_t it = 0; it < SeAssetManager::LEDGER_CAPACITY && resultIndex == SIZE_MAX; it++)
            {
                for (size_t ledgerIt = 0; ledgerIt < SeAssetManager::LEDGER_ENTRY_SIZE_BITS; ledgerIt++)
                {
                    const bool isOccupied = (g_assetManager.ledger[it] & (1ull << ledgerIt)) != 0;
                    if (!isOccupied)
                    {
                        resultIndex = it * SeAssetManager::LEDGER_ENTRY_SIZE_BITS + ledgerIt;
                        break;
                    }
                }
            }
            se_assert_msg(resultIndex != SIZE_MAX, "Not enough space for a new asset");

            const size_t ledgerEntryIndex = resultIndex / SeAssetManager::LEDGER_ENTRY_SIZE_BITS;
            const size_t ledgerEntryBit = 1ull << (resultIndex % SeAssetManager::LEDGER_ENTRY_SIZE_BITS);
            g_assetManager.ledger[ledgerEntryIndex] |= ledgerEntryBit;

            return { resultIndex, &g_assetManager.entries[resultIndex] };
        }

        void remove_entry(size_t index)
        {
            g_assetManager.ledger[index / SeAssetManager::LEDGER_ENTRY_SIZE_BITS] &= ~(1ull << (index % SeAssetManager::LEDGER_ENTRY_SIZE_BITS));
        }

        inline SeAssetHandle encode(SeAssetCategory::Type category, size_t index)
        {
            return SeAssetHandle(category) | (SeAssetHandle(index) << 8);
        }

        inline struct { SeAssetCategory::Type category; size_t index; } decode(SeAssetHandle handle)
        {
            return
            {
                SeAssetCategory::Type(handle & 0xFF),
                size_t(handle >> 8),
            };
        }

        void ensure_capacity(size_t requiredCpuSpace, size_t requiredGpuSpace)
        {
            const size_t availableCpuSpace = g_assetManager.maxCpuUsage - g_assetManager.currentCpuUsage;
            const size_t availableGpuSpace = g_assetManager.maxGpuUsage - g_assetManager.currentGpuUsage;
            size_t cpuSpaceToFree = requiredCpuSpace > availableCpuSpace ? requiredCpuSpace - availableCpuSpace : 0;
            size_t gpuSpaceToFree = requiredGpuSpace > availableGpuSpace ? requiredGpuSpace - availableGpuSpace : 0;

            while (cpuSpaceToFree | gpuSpaceToFree)
            {
                se_assert_msg(g_assetManager.maximizedAssetsQueue, "Not enough space for an asset");

                // Get least recently used asset and minimize it
                SeAssetEntry* const entryToMinimize = g_assetManager.maximizedAssetsQueue;
                entryToMinimize->minimize(entryToMinimize->data);

                // Calculate freed space
                const size_t freedCpuSpace = entryToMinimize->cpuUsageMax - entryToMinimize->cpuUsageMin;
                const size_t freedGpuSpace = entryToMinimize->gpuUsageMax - entryToMinimize->gpuUsageMin;
                g_assetManager.currentCpuUsage -= freedCpuSpace;
                g_assetManager.currentGpuUsage -= freedGpuSpace;
                cpuSpaceToFree = cpuSpaceToFree > freedCpuSpace ? cpuSpaceToFree - freedCpuSpace : 0;
                gpuSpaceToFree = gpuSpaceToFree > freedGpuSpace ? gpuSpaceToFree - freedGpuSpace : 0;

                // Remove minimized asset from maximized asset queue
                if (entryToMinimize->queueNext == entryToMinimize)
                {
                    se_assert(entryToMinimize->queuePrev == entryToMinimize);
                    g_assetManager.maximizedAssetsQueue = nullptr;
                }
                else
                {
                    SeAssetEntry* const next = entryToMinimize->queueNext;
                    SeAssetEntry* const prev = entryToMinimize->queuePrev;
                    next->queuePrev = prev;
                    prev->queueNext = next;
                    g_assetManager.maximizedAssetsQueue = next;
                }
                entryToMinimize->queueNext = nullptr;
                entryToMinimize->queuePrev = nullptr;
            }
        }
    }

    template<typename AssetType>
    SeAssetHandle add(const typename AssetType::Info& info)
    {
        const auto [index, entry] = impl::add_entry();

        if constexpr (!std::is_same_v<typename AssetType::Intermediate, void>)
        {
            typename AssetType::Intermediate intermediateData = AssetType::get_intermediate_data(info);

            const uint32_t requiredCpu = AssetType::count_cpu_usage_min(info, intermediateData);
            const uint32_t requiredGpu = AssetType::count_gpu_usage_min(info, intermediateData);
            impl::ensure_capacity(requiredCpu, requiredGpu);

            *entry =
            {
                requiredCpu,
                requiredGpu,
                AssetType::count_cpu_usage_max(info, intermediateData),
                AssetType::count_gpu_usage_max(info, intermediateData),
                AssetType::load(info, intermediateData),
                AssetType::minimize,
                AssetType::maximize,
                AssetType::unload,
                nullptr,
                nullptr,
            };
            g_assetManager.currentCpuUsage += requiredCpu;
            g_assetManager.currentGpuUsage += requiredGpu;
        }
        else
        {
            static_assert(!"todo");
        }

        return impl::encode(AssetType::CATEGORY, index);
    }

    template<typename AssetType>
    typename AssetType::Value* access(SeAssetHandle handle)
    {
        const auto [category, index] = impl::decode(handle);
        se_assert(category == AssetType::CATEGORY);

        SeAssetEntry* const entry = &g_assetManager.entries[index];
        const bool isAlreadyMaximized = entry->queueNext != nullptr;
        if (isAlreadyMaximized)
        {
            se_assert(entry->queuePrev != nullptr);
            se_assert(g_assetManager.maximizedAssetsQueue);

            // If entry is already maximized and queue has other assets we need to move entry to the top of the queue
            if (entry->queueNext != entry)
            {
                se_assert(entry->queuePrev != entry);

                // Remove entry from queue
                SeAssetEntry* const next = entry->queueNext;
                SeAssetEntry* const prev = entry->queuePrev;
                next->queuePrev = prev;
                prev->queueNext = next;
                if (g_assetManager.maximizedAssetsQueue == entry) g_assetManager.maximizedAssetsQueue = next;

                // Insert entry in the end of queue
                SeAssetEntry* const firstInQueue = g_assetManager.maximizedAssetsQueue;
                SeAssetEntry* const lastInQueue = g_assetManager.maximizedAssetsQueue->queuePrev;
                lastInQueue->queueNext = entry;
                firstInQueue->queuePrev = entry;
                entry->queueNext = firstInQueue;
                entry->queuePrev = lastInQueue;
            }
        }
        else
        {
            // If entry was not maximized whe need to maximize it and add to the end of the queue
            se_assert(entry->queuePrev == nullptr);
            impl::ensure_capacity(entry->cpuUsageMax, entry->gpuUsageMax);
            AssetType::maximize(entry->data);

            if (!g_assetManager.maximizedAssetsQueue)
            {
                // No assets were maximized
                g_assetManager.maximizedAssetsQueue = entry;
                entry->queueNext = entry;
                entry->queuePrev = entry;
            }
            else
            {
                SeAssetEntry* const firstInQueue = g_assetManager.maximizedAssetsQueue;
                SeAssetEntry* const lastInQueue = g_assetManager.maximizedAssetsQueue->queuePrev;
                lastInQueue->queueNext = entry;
                firstInQueue->queuePrev = entry;
                entry->queueNext = firstInQueue;
                entry->queuePrev = lastInQueue;
            }

            g_assetManager.currentCpuUsage += entry->cpuUsageMax - entry->cpuUsageMin;
            g_assetManager.currentGpuUsage += entry->gpuUsageMax - entry->gpuUsageMin;
        }

        return (typename AssetType::Value*)entry->data;
    }

    void remove(SeAssetHandle handle)
    {
        const auto [category, index] = impl::decode(handle);

        SeAssetEntry& entry = g_assetManager.entries[index];
        entry.unload(entry.data);
        impl::remove_entry(index);
        entry = { };
    }

    void engine::init(const SeSettings& settings)
    {
        g_assetManager.maxCpuUsage = settings.maxAssetsCpuUsage;
        g_assetManager.maxGpuUsage = settings.maxAssetsGpuUsage;
    }

    void engine::terminate()
    {
        for (size_t entryIt = 0; entryIt < SeAssetManager::CAPACITY; entryIt++)
        {
            const size_t ledgerEntryIndex = entryIt / SeAssetManager::LEDGER_ENTRY_SIZE_BITS;
            const size_t ledgerEntryBit = 1ull << (entryIt % SeAssetManager::LEDGER_ENTRY_SIZE_BITS);
            const bool isLoaded = g_assetManager.ledger[ledgerEntryIndex] & ledgerEntryBit;
            if (isLoaded)
            {
                SeAssetEntry& entry = g_assetManager.entries[entryIt];
                entry.unload(entry.data);
            }
        }
    }
}

#include "assets/se_mesh_asset.cpp"
