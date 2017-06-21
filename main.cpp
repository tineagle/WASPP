#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstring>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <functional>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const int WIDTH = 800;
const int HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_LUNARG_standard_validation"
};

template <typename T>
class VDeleter {
public:
    VDeleter() : VDeleter([](T, VkAllocationCallbacks*) {}) {};

    VDeleter(std::function<void(T, VkAllocationCallbacks*)> deletef) {
        this->deleter = [=](T obj) { deletef(obj, nullptr); };
    }

    VDeleter(const VDeleter<VkInstance>& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> deletef) {
        this->deleter = [&instance, deletef](T obj) { deletef(instance, obj, nullptr); };
    }

    VDeleter(const VDeleter<VkDevice>& device, std::function<void(VkDevice, T, VkAllocationCallbacks*)> deletef) {
        this->deleter = [&device, deletef](T obj) { deletef(device, obj, nullptr); };
    }

    ~VDeleter() {
        cleanup();
    }

    const T* operator &() const {
        return &object;
    }

    T* replace() {
        cleanup();
        return &object;
    }

    operator T() const {
        return object;
    }

    void operator =(T rhs) {
        if(rhs != object) {
            cleanup();
            object = rhs;
        }
    }

    template<typename V>
    bool operator ==(V rhs) {
        return object == T(rhs);
    }

private:
    T object{VK_NULL_HANDLE};
    std::function<void(T)> deleter;

    void cleanup() {
        if (object != VK_NULL_HANDLE) {
            deleter(object);
        }
        object = VK_NULL_HANDLE;
    }
};

class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
    }

private:
    GLFWwindow* window;
    VDeleter<VkInstance> instance {vkDestroyInstance};
    VDeleter<VkDebugReportCallbackEXT> callback {instance, DestroyDebugReportCallbackEXT};

    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    bool checkInstanceExtensionSupport(const uint32_t extensionCount, const char** extensions) {
        uint32_t aExtensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &aExtensionCount, nullptr);
        std::vector<VkExtensionProperties> aExtensions(aExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &aExtensionCount, aExtensions.data());

        std::cout << "Checking for " << extensionCount << " extension(s)..." << std::endl;
        bool foundExtensions = true;
        for(auto i = 0; i < extensionCount; ++i) {
            std::cout << "Checking for: " << extensions[i] << "... ";
            bool foundExtension = false;
            for(const auto& extension : aExtensions) {
                if(strcmp(extensions[i], extension.extensionName) == 0) {
                    std::cout << "FOUND" << std::endl;
                    foundExtension = true;
                    break;
                }
            }
            if(!foundExtension) {
                std::cout << "FAILED" << std::endl;
                foundExtensions = false;
            }
        }
        if(foundExtensions) {
            std::cout << "Found all extensions" << std::endl;
        } else {
            std::cout << "Failed to find all extensions" << std::endl;
        }

        return foundExtensions;
    }

    VkApplicationInfo makeApplicationInfo(
        const char* applicationName, uint32_t applicationVersion,
        const char* engineName, uint32_t engineVersion,
        uint32_t apiVersion
    ) {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = applicationName;
        appInfo.applicationVersion = applicationVersion;
        appInfo.pEngineName = engineName;
        appInfo.engineVersion = engineVersion;
        appInfo.apiVersion = apiVersion;
        return appInfo;
    }

    VkInstanceCreateInfo makeInstanceCreateInfo(
        const VkApplicationInfo* appInfo,
        uint32_t extensionCount, const char* const* extensions,
        uint32_t layerCount, const char* const* layers
    ) {
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = appInfo;
        createInfo.enabledExtensionCount = extensionCount;
        createInfo.ppEnabledExtensionNames = extensions;
        createInfo.enabledLayerCount = layerCount;
        createInfo.ppEnabledLayerNames = layers;
        return createInfo;
    }

    bool checkInstanceLayerSupport(uint32_t layerCount, const char* const* layers) {
        uint32_t aLayerCount;
        vkEnumerateInstanceLayerProperties(&aLayerCount, nullptr);
        std::vector<VkLayerProperties> aLayers(aLayerCount);
        vkEnumerateInstanceLayerProperties(&aLayerCount, aLayers.data());

        std::cout << "Checking for " << layerCount << " layer(s)..." << std::endl;
        bool foundLayers = true;
        for(uint32_t i = 0; i < layerCount; ++i) {
            std::cout << "Checking for: " << layers[i] << "... ";
            bool foundLayer = false;
            for(const auto& aLayer : aLayers) {
                if(strcmp(layers[i], aLayer.layerName) == 0) {
                    std::cout << "FOUND" << std::endl;
                    foundLayer = true;
                    break;
                }
            }
            if(!foundLayer) {
                std::cout << "FAILED" << std::endl;
                foundLayers = false;
            }
        }
        if(foundLayers) {
            std::cout << "Found all layers" << std::endl;
        } else {
            std::cout << "Failed to find all layers" << std::endl;
        }

        return foundLayers;
    }

    std::vector<const char*> getRequiredExtensions() {
        std::vector<const char*> extensions;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        extensions.insert(extensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);

        if(enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }

        return extensions;
    }

    std::vector<const char*> getRequiredLayers() {
        std::vector<const char*> layers;

        if(enableValidationLayers) {
            layers.insert(layers.begin(), validationLayers.begin(), validationLayers.end());
        }

        return layers;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t obj,
        size_t location,
        int32_t code,
        const char* layerPrefix,
        const char* msg,
        void* userData) {

        std::cerr << "validation layer: " << msg << std::endl;

        return VK_FALSE;
    }

    void createInstance() {
        VkApplicationInfo appInfo = makeApplicationInfo(
            "Hello Triangle", VK_MAKE_VERSION(1, 0, 0),
            "No Engine", VK_MAKE_VERSION(1, 0, 0),
            VK_API_VERSION_1_0);

        std::vector<const char*> extensions;
        std::vector<const char*> layers;

        auto requiredExtensions = getRequiredExtensions();
        auto requiredLayers = getRequiredLayers();

        if(!checkInstanceExtensionSupport(requiredExtensions.size(), requiredExtensions.data())) {
            throw std::runtime_error("an extension was requested but not available");
        }
        if(!checkInstanceLayerSupport(requiredLayers.size(), requiredLayers.data())) {
            throw std::runtime_error("a validation layer was requested but not available");
        }

        extensions.insert(extensions.end(), requiredExtensions.begin(), requiredExtensions.end());
        layers.insert(layers.end(), requiredLayers.begin(), requiredLayers.end());

        VkInstanceCreateInfo createInfo = makeInstanceCreateInfo(
            &appInfo,
            extensions.size(), extensions.data(),
            layers.size(), layers.data());


        if(vkCreateInstance(&createInfo, nullptr, instance.replace()) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    VkDebugReportCallbackCreateInfoEXT makeDebugReportCallbackCreateInfo() {
        VkDebugReportCallbackCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        createInfo.pfnCallback = debugCallback;

        return createInfo;
    }

    void setupDebugCallback() {
        if(!enableValidationLayers) {
            return;
        }
        auto createInfo = makeDebugReportCallbackCreateInfo();
        if(CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, callback.replace()) != VK_SUCCESS) {
            throw std::runtime_error("failed to setup debug callback");
        }
    }

    VkResult CreateDebugReportCallbackEXT(
        VkInstance instance,
        const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugReportCallbackEXT* pCallback)
    {
        auto func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
        if(func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pCallback);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    static void DestroyDebugReportCallbackEXT(
        VkInstance instance,
        VkDebugReportCallbackEXT callback,
        const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
        if(func != nullptr) {
            func(instance, callback, pAllocator);
        }
    }

    void initVulkan() {
        createInstance();
        setupDebugCallback();
    }

    void mainLoop() {
        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
        glfwDestroyWindow(window);

        glfwTerminate();
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
