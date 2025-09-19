# TODO List - LCDproc G15

This file contains all TODO comments found in the source code with explanations and priorities.

## 🔴 Critical (High Priority)

### Server Core Issues

#### `/server/main.c:784` - Client Connection Threading

```c
/* TODO: Move this call to every client connection
```

**Explanation:** Critical threading issue. A function call should occur per client connection, currently presumably only once globally. This can lead to race conditions and inconsistent behavior with multiple simultaneous clients.

**Impact:** Thread-Safety, Multi-Client-Support

#

#### `/server/main.c:824` - Component Dependencies

```c
/* TODO: These things shouldn't be so interdependent.  The order
```

**Explanation:** Architecture problem with circular or fragile dependencies between components. The initialization order is critical and error-prone.

**Impact:** Code maintainability, refactoring resistance

#

#### `/server/main.c:51` - Missing Includes

```c
/* TODO: fill in what to include otherwise */
```

**Explanation:** Incomplete include statements, possibly platform-specific missing header files.

**Impact:** Portability, compilability

#

### Rendering System

#### `/server/render.c:19` - String Rendering Correctness

```c
* \todo Review render_string for correctness.
```

**Explanation:** The `render_string` function requires code review for correctness. Since this is a core function for text display, bugs here can lead to display errors.

**Impact:** Display quality, text rendering

#

#### `/server/render.c:377` - Left-Extending Widgets

```c
/* TODO:  Rearrange stuff to get left-extending
```

**Explanation:** Incomplete implementation for widgets that extend to the left. This affects layout and positioning.

**Impact:** Widget layout, UI design

#

#### `/server/render.c:405` - Down-Extending Widgets

```c
/* TODO:  Rearrange stuff to get down-extending
```

**Explanation:** Similar problem for vertically downward-extending widgets.

**Impact:** Widget layout, UI design

#

## 🟡 Medium (Medium Priority)

### Menu System

#### `/server/menu.c:374` - Frame-Based Scrolling

```c
/* TODO: Put menu in a frame to do easy scrolling */
```

**Explanation:** Menu scrolling should be implemented in frames for better performance and clean architecture.

**Impact:** Performance, code architecture

#

#### `/server/menu.c:453,524,544` - Rendering Safety ✅ **PARTIALLY FIXED**

```c
/* TODO: when menu is in a frame, these can be removed */
/* TODO: remove next 3 lines when rendering is safe */
```

**Explanation:** ~~Multiple workarounds in menu rendering that can be removed once the rendering system is safer.~~ **UPDATED 2025-09:** Rendering safety significantly improved through NULL-pointer fixes. Added proper NULL checks for widgets before accessing their properties (w->y, w->x), preventing crashes and making rendering more robust.

**Fixes Applied:**

- Added NULL-check with `return` for title widget (line 491)
- Added NULL-check with `continue` for text widgets (line 520)
- Added NULL-check with `break` for icon widgets (line 537)

**Impact:** ~~Code cleanup, performance~~ **Improved stability, crash prevention**

#

#### `/server/menu.c:795` - Numeric Selection

```c
/* TODO: move to the selected number and enter it */
```

**Explanation:** Feature for direct numeric menu navigation is not implemented.

**Impact:** User experience

#

#### `/server/menuscreens.c:490` - Screen Menu Functions

```c
* TODO: Menu items in the screens menu currently have no functions
```

**Explanation:** Menu items in the screen menu are dummy entries without functionality.

**Impact:** Functionality, completeness

#

### Client Issues

#### `/clients/lcdproc/machine_Linux.c:74` - Buffer Hack

```c
static char procbuf[1024]; /* TODO ugly hack! */
```

**Explanation:** Static buffer with fixed size is an unclean solution that can lead to buffer overflows.

**Impact:** Security, code quality

#

#### `/clients/lcdproc/machine_Linux.c:353,397` - Error Reporting

```c
perror("getloadavg"); /* ToDo: correct error reporting */
/* ToDo: correct error reporting */
```

**Explanation:** Error handling uses primitive `perror()` instead of the report system.

**Impact:** Error handling, consistency

#

#### `/clients/lcdexec/lcdexec.c:498` - Better Implementation

```c
// TODO: make it better
```

**Explanation:** Code location (server "bye" handling) needs implementation improvement.

**Impact:** Code quality

#

### Rendering Details

#### `/server/render.c:253` - Horizontal Frame Scrolling

```c
/* TODO:  Frames don't scroll horizontally yet! */
```

**Explanation:** Horizontal scrolling in frames is not implemented.

**Impact:** Feature completeness

#

#### `/server/render.c:266` - Cleaner Implementation

```c
/* TODO:  Make this cleaner and more flexible! */
```

**Explanation:** Code location needs refactoring for better flexibility.

**Impact:** Code quality, extensibility

#

## 🟢 Low (Low Priority)

### Documentation & API

#### `/server/commands/client_commands.c:59` - Server Info

```c
* \todo  Give \em real info about the server/lcd
```

**Explanation:** `hello_func()` should return real server/LCD information instead of dummy data.

**Impact:** API completeness

#

#### `/shared/sring.c:4` - Missing Functions

```c
* \todo Implement sring_peek() and sring_skip().
```

**Explanation:** String ring buffer library has missing utility functions.

**Impact:** API completeness

#

### Error Handling

#### `/server/client.c:211` - Error Checking

```c
/* TODO:  Check for errors here?*/
```

**Explanation:** Missing error handling at an undetermined location in client code.

**Impact:** Robustness

#

#### `/clients/lcdproc/machine_Linux.c:467` - Error Checking

```c
/* TODO:  Check for errors here? */
```

**Explanation:** Similar missing error handling in Linux-specific code.

**Impact:** Robustness

#

### Testing

#### `/tests/test_integration_g15.c:863` - Hardware Testing

```c
/* TODO: Add actual G15 driver tests with mock hardware when G15 driver is configured */
```

**Explanation:** G15 hardware tests with mock hardware are not implemented.

**Impact:** Test coverage

#

#### `/shared/LL.c:26` - Library Testing

```c
// TODO: Test everything?
```

**Explanation:** Linked list library needs comprehensive tests.

**Impact:** Test coverage

#

### Server Screens

#### `/server/serverscreens.c:149` - Unfinished Implementation

```c
/* TODO:
```

**Explanation:** Incomplete TODO comment indicates unfinished implementation.

**Impact:** Code completeness

#

---

## Summary

- **Total TODOs at 09/2025:** 20 (1 partially fixed)
- **Critical:** 6 (Threading, Dependencies, Rendering)
- **Medium:** 9 active + 1 partially fixed (Menus, Clients, Rendering)
- **Low:** 4 (Documentation, Testing)

**Recent Fixes (September 2025):**

- ✅ **Menu rendering safety partially improved** through NULL-pointer checks
- ✅ **Memory leak fixes** in multiple components (sring.c, linux_input.c, menuitem.c)
- ✅ **Null-pointer dereference prevention** across server components
- ✅ **Enum cast out-of-range fixes** for better type safety

**Recommended Order:**

1. Server Core Issues (Threading, Dependencies)
2. Rendering System Correctness
3. Menu System Improvements
4. Client-Side Fixes
5. Documentation & Testing
