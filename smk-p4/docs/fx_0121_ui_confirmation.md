# UI Parameter Confirmation Contract
## SMK-P4 0.12.1

### Architecture
The UI Blueprint library interacts with the performance backend strictly through the `IUiBackend` interface (`smk-p4/components/ui_blueprint/include/ui_backend.h`) and `UiBlueprintPerformanceAdapter`.

UI commands follow an asynchronous **Request-Verify-Confirm** lifecycle:

```
[UI Screen / Slider] 
         |
         | requestParameterFloat("fx.delay.mix", 50.0f)
         v
[UiBlueprintPerformanceAdapter] 
         | 
         | -> returns UiCommandState::Requested
         | -> captures fx_pending_token_ = backend.fxToken()
         | -> captures initial errors and rejection counters
         v
[PerformanceBackend / Audio Thread]
         |
         | -> applies parameter, publishes snapshot with fx.request >= fx_pending_token_
         v
[UiBlueprintPerformanceAdapter::service(now_ms)]
         |
         |-> checks snapshot fx->request >= fx_pending_token_
         |-> transitions to UiCommandState::Confirmed
         |-> updates UiModel with owner-confirmed value
```

### Lifecycle States
1. **Requested**: Returned immediately upon touch or slider event. Indicates command has been queued for execution.
2. **Confirmed**: Set when a published engine snapshot confirms `fx->request >= fx_pending_token_` with no increment in errors or rejections. The confirmed DSP value is mirrored into `UiModel`.
3. **Rejected**: Triggered if:
   - Backend is busy with patch switching or preset storage.
   - Owner reports incremented error count (`fx->errors > fx_initial_errors_`).
   - Owner reports incremented rejection count (`fx->rejected > fx_initial_rejected_`).
   - Parameter value is out of supported range (e.g., NaN or feedback > 0.95).
   - In rejection state, the UI model restores the owner's confirmed value (`"FX REJECTED; CONFIRMED VALUES RESTORED"`).
4. **Timeout**: If the engine fails to confirm within `kPatchConfirmationTimeoutMs` (2000 ms), the adapter marks the command as `UiCommandState::Rejected` (`"FX NOT CONFIRMED; OWNER STATE RETAINED"`) and re-synchronizes the model with the last confirmed snapshot.

### Monotonic Token Tracking
Every parameter change increments `backend.fxToken()`. When the user rapidly drags a slider:
- Each intermediate value generates an incremented token.
- Intermediate requests are processed sequentially by the wait-free ring buffer `FxQueue<8>`.
- The UI adapter only confirms the final state once the engine reaches or exceeds the latest requested token.
