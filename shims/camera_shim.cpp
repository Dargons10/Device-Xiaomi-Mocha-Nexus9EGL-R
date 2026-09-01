/*
 * Copyright (C) 2017 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>

#include <ui/GraphicBuffer.h>

extern "C" void _ZN7android13GraphicBufferC1EjjijjjP13native_handleb(
        void* this_ptr,
        uint32_t width,
        uint32_t height,
        int format,
        uint32_t usage,
        uint32_t stride,
        uint32_t layerCount,
        native_handle_t* handle,
        bool keepOwnership);

extern "C" void _ZN7android13GraphicBufferC1EjjijjP13native_handleb(
        void* this_ptr,
        uint32_t width,
        uint32_t height,
        int format,
        uint32_t usage,
        uint32_t stride,
        native_handle_t* handle,
        bool keepOwnership)
{
    _ZN7android13GraphicBufferC1EjjijjjP13native_handleb(
        this_ptr, width, height, format, usage, stride, 1, handle, keepOwnership);
}

