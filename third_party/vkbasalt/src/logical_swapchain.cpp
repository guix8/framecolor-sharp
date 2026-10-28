#include "logical_swapchain.hpp"

namespace vkBasalt
{
    void LogicalSwapchain::destroy()
    {
        if (imageCount > 0)
        {
            // The last effect submits may still read the effect resources.
            pLogicalDevice->vkd.QueueWaitIdle(pLogicalDevice->queue);
            effects.clear();

            pLogicalDevice->vkd.FreeCommandBuffers(
                pLogicalDevice->device, pLogicalDevice->commandPool, commandBuffersEffect.size(), commandBuffersEffect.data());
            Logger::debug("after free commandbuffer");

            pLogicalDevice->vkd.FreeMemory(pLogicalDevice->device, fakeImageMemory, nullptr);

            for (uint32_t i = 0; i < fakeImages.size(); i++)
            {
                pLogicalDevice->vkd.DestroyImage(pLogicalDevice->device, fakeImages[i], nullptr);
            }

            for (unsigned int i = 0; i < imageCount; i++)
            {
                pLogicalDevice->vkd.DestroySemaphore(pLogicalDevice->device, semaphores[i], nullptr);
            }
            Logger::debug("after DestroySemaphore");
        }
    }
} // namespace vkBasalt
