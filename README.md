## Locky Gizmo
                                                       
The Locky Gizmo is an appliance with lots of locks and latches for a kid (ages 2-90) to play with. This Arduino sketch runs the electronic latch on one of the doors.

The sketch monitors a number of buttons for interaction. For the purpose of unlocking the door, each button behaves as a toggle. This means we adjust behavior for momentary switched to make them flip-flops. When all toggles are activated, the door is unlocked. An LED for each toggle shows status.

The master switch provides power to the board, but has an accompanying power LED as well. The LED is on a digital output so it can be slept.

Tone feedback escalates as more toggles are activated. When all toggles are activated, a servo moves to unlock a door. A door switch helps us know when to re-lock the servo and how to initialize the servo on power-on.

I used the Arduino Pro Mini in my implementation. While this sketch should work on most Arduino boards, note different pinouts and worse sleep mode power consumption if using a Pro Micro, for example.

## License

[MIT License](http://www.opensource.org/licenses/MIT).