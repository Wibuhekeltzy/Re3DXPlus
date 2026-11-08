#ifndef STUB_VK_H
#define STUB_VK_H
typedef struct VkInstance_T *VkInstance;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
#define VK_NULL_HANDLE ((void *)0)
#define VK_SUCCESS 0
#define VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR 1000008000
#define VK_KHR_SURFACE_EXTENSION_NAME "VK_KHR_surface"
#define VK_KHR_ANDROID_SURFACE_EXTENSION_NAME "VK_KHR_android_surface"
struct ANativeWindow;
typedef struct { int sType; const void *pNext; unsigned flags; struct ANativeWindow *window; } VkAndroidSurfaceCreateInfoKHR;
int vkCreateAndroidSurfaceKHR(VkInstance, const VkAndroidSurfaceCreateInfoKHR *, const void *, VkSurfaceKHR *);
#endif
