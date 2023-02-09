## DragLights
An Arduino sketch to implement Drag Racing (Christmas Tree) starter lights.

This code accompanies the blog article at the [Arduino++ blog](https://arduinoplusplus.wordpress.com/2023/02/09/drag-race-start-lights/)

DragLights encodes the rules below with the 'lamps' implemented as NeoPixel LEDs managed by the FastLED library.

- One active low digital input switch is used to start/reset the lights sequence.
- Pairs of active low digital inputs are used to signal the prestage and staged optical 
  beams per racer.
