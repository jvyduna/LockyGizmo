## Locky Gizmo
                                                       
The Locky Gizmo is an appliance with lots of locks and latches for a kid (ages 2-90) to play with. This Arduino sketch runs the electronic latch on one of the doors.

  ![](https://raw.github.com/jvyduna/LockyGizmo/master/images/Locky-Gizmo-Latch-Board.gif)

## See it in action

  https://www.youtube.com/watch?v=9oBQnGuzx6Q

## Description

The sketch monitors a number of buttons for interaction. For the purpose of unlocking the door, each button behaves as a toggle. This means we adjust behavior for momentary switches to make them flip-flops. When all toggles are activated, the door is unlocked. An LED for each toggle shows status.

The master switch provides power to the board, but has an accompanying power LED as well. The LED is on a digital output so it can be slept.

Tone feedback escalates as more toggles are activated. When all toggles are activated, a servo moves to unlock a door. A door position proximity switch helps us know when to re-lock the servo and how to initialize the servo on power-on.

I used the Arduino Pro Mini in my implementation. While this sketch should work on most Arduino boards, note different pinouts and worse sleep mode power consumption if using a Pro Micro, for example.

## You can help

### Code
  1. My C is lacking. Teach me something with a pull request.
  2. Add a harder mode where the order of activation matters.
  3. Refactor to have button and LEDs pins alternating, instead of on opposite sides of the board. This might be easier to wire up.
  4. Make it such that toggles are always reset to "inactive" when the door is opened and re-closed, just like the momentary switches do. I.E. flipp them between a normally open and normally closed assumption.

### Hardware
  1. If you're in posession of my initial hardware, you'll see it uses a physical proximity door position switch that, in beta testing with a two year old, encourages two year olds to grasp and destroy it. Replace this with a Hall effect sensor or magnetic reed switch.
  2. The internal brass release might be easier with a pull cord, perhaps coming up through the plumbing ball valve on top. But maybe it doesn't need to be easier.
  3. Rub-in dark stain marker / nick repair.
  4. Replace the shoddy peep-hole and articulating flashlight on the back, above the electronics. The intent was to let you peep in and discover the hidden internal brass door latch. 


## License

[MIT License](http://www.opensource.org/licenses/MIT)