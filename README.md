# Analog Glove

A work-in-progress, BLE-enabled analog input glove using flex sensors, real-time ADC sampling, and USB HID output.  
Currently, the core features are being developed and validated through smaller experimental projects, as follows:

- [ ] BLE (Mostly complete) [Notion Page](https://jolly-cycle-c67.notion.site/Analog-Glove-228ed72549008087aae9fa4d90f2193a?pvs=74)
- [ ] ADC (In progress)
- [ ] HID (In progress)
- [ ] CLI 

- [`ble_app_template`](./examples/jesica/ble_app_template):
  Targeted for the **nRF52840 Dongle**. 
- [`ble_app_gatts`](./examples/jesica/ble_app_gatts):  
  Tailored for the **SuperMini nRF52840**.
  
# Main core logic inspiration

1. [DIYAnalogKeeb by tommybee456](https://github.com/tommybee456/DIYAnalogKeeb/tree/main/src)  
2. [macrolev by heiso](https://github.com/heiso/macrolev)

# Status

> This is a **theoretical and experimental** project in active development. Hardware and firmware components are subject to change as prototypes evolve.

---
