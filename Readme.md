# [WIP] 泉州云卓 C12 双光云台控制接口 ROS 层驱动

## Install

### Dependencies

1. [BIT-SGC/C12_GimbalControl](https://github.com/BIT-SGC/C12_GimbalControl)

Download .deb package from the repository


```bash
sudo apt-get install ./gimbal_drv.deb
```

## TODO-LIST

- [ ] msg 从 gimbal_rc_wrapper 转移到 gimbal_driver_ros
- [ ] 录像触发逻辑清理 (转移到 gimbal_rc_wrapper, 包中仅做触发控制)
