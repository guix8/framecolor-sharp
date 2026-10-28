#include "effect_lut.hpp"

#include <cstring>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <cstdlib>

#include "image_view.hpp"
#include "descriptor_set.hpp"
#include "buffer.hpp"
#include "renderpass.hpp"
#include "graphics_pipeline.hpp"
#include "framebuffer.hpp"
#include "shader.hpp"
#include "sampler.hpp"
#include "image.hpp"
#include "lut_cube.hpp"

#include "shader_sources.hpp"

namespace vkBasalt
{
    LutEffect::LutEffect(LogicalDevice*       pLogicalDevice,
                         VkFormat             format,
                         VkExtent2D           imageExtent,
                         std::vector<VkImage> inputImages,
                         std::vector<VkImage> outputImages,
                         Config*              pConfig)
    {
        vertexCode   = full_screen_triangle_vert;
        fragmentCode = lut_frag;

        std::string lutFile = pConfig->lutFile();

        // FrameColor generates only .cube LUTs; PNG/image loaders are omitted.
        LutCube lutCube(lutFile);
        int32_t height = lutCube.size;
        const unsigned char* pixels = lutCube.colorCube.data();
        VkSpecializationMapEntry specMapEntry{0, 0, sizeof(int32_t)};

        VkSpecializationInfo fragmentSpecializationInfo;
        fragmentSpecializationInfo.mapEntryCount = 1;
        fragmentSpecializationInfo.pMapEntries   = &specMapEntry;
        fragmentSpecializationInfo.dataSize      = sizeof(height);
        fragmentSpecializationInfo.pData         = &height;

        pVertexSpecInfo   = nullptr;
        pFragmentSpecInfo = &fragmentSpecializationInfo;

        VkExtent3D lutImageExtent = {(uint32_t) height, (uint32_t) height, (uint32_t) height};
        lutSize = static_cast<uint32_t>(height);

        lutImage = createImages(pLogicalDevice,
                                1,
                                lutImageExtent,
                                VK_FORMAT_R8G8B8A8_UNORM, // TODO search for format and save it
                                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                lutMemory)[0];

        uploadToImage(pLogicalDevice, lutImage, lutImageExtent, height * height * height * 4, pixels);

        lutImageView = createImageViews(pLogicalDevice, VK_FORMAT_R8G8B8A8_UNORM, std::vector<VkImage>(1, lutImage), VK_IMAGE_VIEW_TYPE_3D)[0];

        lutDescriptorSetLayout = createImageSamplerDescriptorSetLayout(pLogicalDevice, 1);
        descriptorSetLayouts.push_back(lutDescriptorSetLayout);

        VkDescriptorPoolSize imagePoolSize;
        imagePoolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        imagePoolSize.descriptorCount = 1;

        std::vector<VkDescriptorPoolSize> poolSizes = {imagePoolSize};

        lutDescriptorPool = createDescriptorPool(pLogicalDevice, poolSizes);

        // The sharpen amount and the compare bypass live in a mapped uniform
        // buffer, so changes do not need new pipelines or command buffers.
        createBuffer(pLogicalDevice,
                     sizeof(float) * 4,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     sharpenBuffer,
                     sharpenMemory);
        VkResult result = pLogicalDevice->vkd.MapMemory(
            pLogicalDevice->device, sharpenMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&pParams));
        ASSERT_VULKAN(result);
        pParams[0] = 0.0f;
        pParams[1] = 0.0f;

        sharpenDescriptorSetLayout = createUniformBufferDescriptorSetLayout(pLogicalDevice);
        descriptorSetLayouts.push_back(sharpenDescriptorSetLayout);

        VkDescriptorPoolSize bufferPoolSize;
        bufferPoolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bufferPoolSize.descriptorCount = 1;

        sharpenDescriptorPool = createDescriptorPool(pLogicalDevice, {bufferPoolSize});

        init(pLogicalDevice, format, imageExtent, inputImages, outputImages, pConfig);

        lutDescriptorSet =
            allocateAndWriteImageSamplerDescriptorSets(pLogicalDevice,
                                                       lutDescriptorPool,
                                                       lutDescriptorSetLayout,
                                                       {sampler},
                                                       std::vector<std::vector<VkImageView>>(1, std::vector<VkImageView>(1, lutImageView)))[0];

        sharpenDescriptorSet = writeBufferDescriptorSet(pLogicalDevice, sharpenDescriptorPool, sharpenDescriptorSetLayout, sharpenBuffer);
    }
    LutEffect::~LutEffect()
    {
        pLogicalDevice->vkd.DestroyImageView(pLogicalDevice->device, lutImageView, nullptr);
        pLogicalDevice->vkd.DestroyImage(pLogicalDevice->device, lutImage, nullptr);
        pLogicalDevice->vkd.DestroyDescriptorSetLayout(pLogicalDevice->device, lutDescriptorSetLayout, nullptr);
        pLogicalDevice->vkd.DestroyDescriptorPool(pLogicalDevice->device, lutDescriptorPool, nullptr);
        pLogicalDevice->vkd.FreeMemory(pLogicalDevice->device, lutMemory, nullptr);
        pLogicalDevice->vkd.UnmapMemory(pLogicalDevice->device, sharpenMemory);
        pLogicalDevice->vkd.DestroyBuffer(pLogicalDevice->device, sharpenBuffer, nullptr);
        pLogicalDevice->vkd.FreeMemory(pLogicalDevice->device, sharpenMemory, nullptr);
        pLogicalDevice->vkd.DestroyDescriptorSetLayout(pLogicalDevice->device, sharpenDescriptorSetLayout, nullptr);
        pLogicalDevice->vkd.DestroyDescriptorPool(pLogicalDevice->device, sharpenDescriptorPool, nullptr);
    }
    void LutEffect::applyEffect(uint32_t imageIndex, VkCommandBuffer commandBuffer)
    {
        const std::array<VkDescriptorSet, 2> descriptorSets{{lutDescriptorSet, sharpenDescriptorSet}};
        pLogicalDevice->vkd.CmdBindDescriptorSets(commandBuffer,
                                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                  pipelineLayout,
                                                  1,
                                                  static_cast<uint32_t>(descriptorSets.size()),
                                                  descriptorSets.data(),
                                                  0,
                                                  nullptr);
        SimpleEffect::applyEffect(imageIndex, commandBuffer);
    }
    void LutEffect::updateEffect()
    {
        // The dashboard writes current.txt during slider drags and on release.
        const auto now = std::chrono::steady_clock::now();
        if (now < nextFrameColorCheck)
            return;
        nextFrameColorCheck = now + std::chrono::milliseconds(50);
        const char* home = std::getenv("HOME");
        if (!home || lutSize < 2 || lutSize > 65)
            return;
        const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
        const std::string configRoot = xdgConfig && *xdgConfig ? xdgConfig : std::string(home) + "/.config";
        std::ifstream valuesFile(configRoot + "/framecolor/current.txt");
        std::array<float, 14> values{{0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
                                      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f}};
        std::vector<float> stored;
        for (float value; valuesFile >> value;)
            stored.push_back(value);
        if (stored.size() < 6 || stored.size() == 11)
            return;
        for (size_t i = 0; i < std::min(stored.size(), values.size()); ++i)
            values[i] = stored[i];
        // Older files have no gain, saturation, sharpen or bypass fields. The
        // ten-value file used one saturation setting for all three output channels.
        if (stored.size() == 10)
            values[10] = values[11] = values[9];
        if (!std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); }))
            return;
        if (hasFrameColorValues && values == frameColorValues)
            return;
        const bool lookChanged =
            !hasFrameColorValues || !std::equal(values.begin(), values.begin() + 12, frameColorValues.begin());
        const bool paramsChanged = !hasFrameColorValues || values[12] != frameColorValues[12]
                                   || values[13] != frameColorValues[13];

        std::vector<unsigned char> pixels;
        if (lookChanged)
        {
            std::array<std::array<unsigned char, 65>, 3> curves{};
            for (uint32_t channel = 0; channel < 3; ++channel)
            {
                float contrast = std::clamp(values[channel], 0.0f, 1.0f);
                float gamma = std::clamp(values[channel + 3], 0.50f, 2.50f);
                float gain = std::clamp(values[channel + 6], 0.50f, 1.50f);
                for (uint32_t point = 0; point < lutSize; ++point)
                {
                    float x = static_cast<float>(point) / static_cast<float>(lutSize - 1);
                    float color = std::pow(std::clamp(x * gain, 0.0f, 1.0f), 1.0f / gamma);
                    float smooth = color * color * (3.0f - 2.0f * color);
                    float corrected = std::clamp(color * (1.0f - contrast) + smooth * contrast, 0.0f, 1.0f);
                    curves[channel][point] = static_cast<unsigned char>(std::lround(corrected * 255.0f));
                }
            }
            pixels.assign(lutSize * lutSize * lutSize * 4, 255);
            const std::array<float, 3> saturation{{std::clamp(values[9], 0.0f, 2.0f),
                                                    std::clamp(values[10], 0.0f, 2.0f),
                                                    std::clamp(values[11], 0.0f, 2.0f)}};
            for (uint32_t blue = 0; blue < lutSize; ++blue)
                for (uint32_t green = 0; green < lutSize; ++green)
                    for (uint32_t red = 0; red < lutSize; ++red)
                    {
                        size_t offset = ((blue * lutSize + green) * lutSize + red) * 4;
                        if (saturation == std::array<float, 3>{{1.0f, 1.0f, 1.0f}})
                        {
                            pixels[offset] = curves[0][red];
                            pixels[offset + 1] = curves[1][green];
                            pixels[offset + 2] = curves[2][blue];
                            continue;
                        }
                        const float r = curves[0][red] / 255.0f;
                        const float g = curves[1][green] / 255.0f;
                        const float b = curves[2][blue] / 255.0f;
                        const float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                        pixels[offset] = static_cast<unsigned char>(std::lround(
                            std::clamp(luminance + (r - luminance) * saturation[0], 0.0f, 1.0f) * 255.0f));
                        pixels[offset + 1] = static_cast<unsigned char>(std::lround(
                            std::clamp(luminance + (g - luminance) * saturation[1], 0.0f, 1.0f) * 255.0f));
                        pixels[offset + 2] = static_cast<unsigned char>(std::lround(
                            std::clamp(luminance + (b - luminance) * saturation[2], 0.0f, 1.0f) * 255.0f));
                    }
        }

        // A previous present can still read this image and buffer. Dashboard
        // writes are throttled, so wait before replacing them in place.
        pLogicalDevice->vkd.QueueWaitIdle(pLogicalDevice->queue);
        if (lookChanged)
        {
            uploadToImage(pLogicalDevice, lutImage, {lutSize, lutSize, lutSize}, static_cast<uint32_t>(pixels.size()),
                          pixels.data(), 1, true);
            Logger::info("FrameColor live LUT updated");
        }
        if (paramsChanged)
        {
            pParams[0] = std::clamp(values[12], 0.0f, 1.0f);
            pParams[1] = values[13] > 0.5f ? 1.0f : 0.0f;
            Logger::info("FrameColor sharpen " + std::to_string(pParams[0]) + " bypass " + std::to_string(pParams[1]));
        }
        frameColorValues = values;
        hasFrameColorValues = true;
    }
} // namespace vkBasalt
