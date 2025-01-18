# kemu

a clean, from-scratch, lightweight full system hypervisor for hosting KVM guests

## build

no special dependence

```bash
make
```

and you will get `src/kemu`

## run

most arguments are the same as qemu, except you need to change `-` to `--`, for example `-kernel`(in qemu) to `--kernel`(in kemu)

if you have already build a linux kernel bzImage and initramfs, start the hypervisor by running the following cmd 

```bash
./src/kemu --kernel <your-bzImage-path> --disk <your-diskimg-path>
```

## reference

- [kvmtool](https://github.com/kvmtool/kvmtool)
- [blogspot david942j](https://david942j.blogspot.com/2018/10/note-learning-kvm-implement-your-own.html)