"""AppleWin's SDL frame without a GL swap interval, as under WSL.

sa2 stops when SDL_GL_SetSwapInterval fails; this falls back to none.

Usage: applewin-patch.py <sdlframe.cpp>
"""

import sys

path = sys.argv[1]
old  = 'throw std::runtime_error(decorateSDLError("SDL_GL_SetSwapInterval"));\n        }\n\n        myTargetGLSwap = *it;'
new  = 'myTargetGLSwap = 0; mySynchroniseWithTimer = options.syncWithTimer; return;\n        }\n\n        myTargetGLSwap = *it;'
text = open(path).read()

if new in text:
    print("sdlframe.cpp: already changed")
elif text.count(old) != 1:
    sys.exit("sdlframe.cpp: the swap interval check isn't there")
else:
    open(path, "w").write(text.replace(old, new))
    print("sdlframe.cpp: changed")
