/*
 * Copyright (C) 2020 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2020 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-ws-lib
 * Created on: 8 сент. 2017 г.
 *
 * lsp-ws-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-ws-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-ws-lib. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/common/types.h>

#include <lsp-plug.in/ws/keycodes.h>
#include <private/win32/decode.h>

namespace lsp
{
    namespace ws
    {
        namespace win32
        {
            size_t decode_mouse_state(size_t vKey)
            {
                size_t result = 0;
                #define DC(mask, flag)  \
                    if (vKey & mask) \
                        result |= flag; \

                DC(MK_SHIFT, MCF_SHIFT);
                DC(MK_CONTROL, MCF_CONTROL);
                DC(MK_ALT, MCF_ALT);
                DC(MK_LBUTTON, MCF_LEFT);
                DC(MK_MBUTTON, MCF_MIDDLE);
                DC(MK_RBUTTON, MCF_RIGHT);

                #undef DC

                return result;
            }

            size_t get_key_state() 
            {
                // If the high-order bit is 1, the key is down; otherwise, it is up.
                // If the low-order bit is 1, the key is toggled.
                // A key, such as the CAPS LOCK key, is toggled if it is turned on. 
                // The key is off and untoggled if the low-order bit is 0. 
                // A toggle key's indicator light (if any) on the keyboard will be on when the key is toggled, and off when the key is untoggled.

                size_t result = 0;
                SHORT keySt = 0;
                #define DC(mask, flag)  \
                    keySt = GetKeyState(mask); \
                    if ((keySt & 0x8000)) \
                        result |= flag; \

                DC(VK_SHIFT, MCF_SHIFT);
                DC(VK_CONTROL, MCF_CONTROL);
                DC(VK_MENU, MCF_ALT);
                DC(VK_LBUTTON, MCF_LEFT);
                DC(VK_MBUTTON, MCF_MIDDLE);
                DC(VK_RBUTTON, MCF_RIGHT);

                #undef DC

                return result;
            }

            code_t decode_keycode(unsigned long code)
            {
                switch (code)
                {
                case VK_BACK:
                    return WSK_BACKSPACE;
                case VK_TAB:
                    return WSK_TAB;
                case VK_RETURN:
                    return WSK_RETURN;
                case VK_CAPITAL:
                    return WSK_CAPS_LOCK;
                case VK_ESCAPE:
                    return WSK_ESCAPE;
                case VK_PRIOR:
                    return WSK_PAGE_UP;
                case VK_NEXT:
                    return WSK_PAGE_DOWN;
                case VK_END:
                    return WSK_END;
                case VK_HOME:
                    return WSK_HOME;
                case VK_LEFT:
                    return WSK_LEFT;
                case VK_UP:
                    return WSK_UP;
                case VK_RIGHT:
                    return WSK_RIGHT;
                case VK_DOWN:
                    return WSK_DOWN;
                case VK_SELECT:
                    return WSK_SELECT;
                case VK_PRINT:
                    return WSK_PRINT;
                case VK_EXECUTE:
                    return WSK_EXECUTE;
                case VK_INSERT:
                    return WSK_INSERT;
                case VK_DELETE:
                    return WSK_DELETE;
                case VK_HELP:
                    return WSK_HELP;
                case VK_NUMPAD0 ... VK_NUMPAD9:
                    return WSK_KEYPAD_0 + (code - VK_NUMPAD0);
                case VK_MULTIPLY:
                    return WSK_KEYPAD_MULTIPLY;
                case VK_ADD:
                    return WSK_KEYPAD_ADD;
                case VK_SEPARATOR:
                    return WSK_KEYPAD_SEPARATOR;
                case VK_SUBTRACT:
                    return WSK_KEYPAD_SUBTRACT;
                case VK_DECIMAL:
                    return WSK_KEYPAD_DECIMAL;
                case VK_DIVIDE:
                    return WSK_KEYPAD_DIVIDE;
                case VK_F1 ... VK_F24:
                    return WSK_F1 + (code - VK_F1);
                case VK_NUMLOCK:
                    return WSK_NUM_LOCK;
                case VK_SCROLL:
                    return WSK_SCROLL_LOCK;
                case 0x30 ... 0x39: // 0 - 9 key
                case 0x41 ... 0x5A: // A - Z key
                {
                    if ((get_key_state() & MCF_CONTROL) == MCF_CONTROL) {
                        return code;
                    }
                }
                default:
                    return WSK_UNKNOWN;
                }
            }
        }
    }
}
