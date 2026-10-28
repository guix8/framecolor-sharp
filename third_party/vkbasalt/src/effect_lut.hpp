#ifndef EFFECT_LUT_HPP_INCLUDED
#define EFFECT_LUT_HPP_INCLUDED
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <array>
#include <chrono>

#include "vulkan_include.hpp"

#include "effect_simple.hpp"
#include "config.hpp"

namespace vkBasalt
{
    class LutEffect : public SimpleEffect
    {
    public:
        LutEffect(LogicalDevice*       pLogicalDevice,
                  VkFormat             format,
                  VkExtent2D           imageExtent,
                  std::vector<VkImage> inputImages,
                  std::vector<VkImage> outputImages,
                  Config*              pConfig);
        ~LutEffect();
        void applyEffect(uint32_t imageIndex, VkCommandBuffer commandBuffer) override;
        void updateEffect() override;

    private:
        VkImage               lutImage;
        VkDeviceMemory        lutMemory;
        VkImageView           lutImageView;
        VkDescriptorSetLayout lutDescriptorSetLayout;
        VkDescriptorPool      lutDescriptorPool;
        VkDescriptorSet       lutDescriptorSet;
        VkBuffer              sharpenBuffer;
        VkDeviceMemory        sharpenMemory;
        float*                pParams = nullptr; // {sharpen amount, bypass}
        VkDescriptorSetLayout sharpenDescriptorSetLayout;
        VkDescriptorPool      sharpenDescriptorPool;
        VkDescriptorSet       sharpenDescriptorSet;
        uint32_t              lutSize = 0;
        bool                  hasFrameColorValues = false;
        std::array<float, 14> frameColorValues{};
        std::chrono::steady_clock::time_point nextFrameColorCheck{};
    };
} // namespace vkBasalt

#endif // EFFECT_LUT_HPP_INCLUDED
