## Libswd

A minimum viable debugger that targets ARM M-Profile processors[^1]

### Requirements

To allow for portability, a hardware abstraction layer class, `SWDDriver`, is provided. An implementation of this class needs to be provided


[^1]: While most of development was done on a Cortex-M4F, because all M-profiles share similar debug architecture, a majority of the implemented features will work. 

