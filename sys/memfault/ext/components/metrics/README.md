# metrics

This API can easily be used to monitor device health over time (i.e.
connectivity, battery life, MCU resource utilization, hardware degradation,
etc.) and configure Alerts with the Memfault backend when things go astray. To
get started, see this [document](https://mflt.io/2D8TRLX).

### Core features

- Allows users to create entries that can be uploaded to the Memfault cloud.
- Coredump info can include interrupt state, SDK info like Memfault log buffers,
  and reset reasons.

### Storage allocations

- `memfault_metrics_boot()`: connects the metrics into the event storage region,
  sets up the metrics timer, starts the timer, and adds an "unexpected reboot"
  counter metric.
