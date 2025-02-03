# kemu

kemu is a lightweight hypervisor for hosting KVM guests

![sdzklj](https://raw.githubusercontent.com/learner-lu/picbed/master/sdzklj.gif)

## build

no special dependence

```bash
make
```

and you will get `kemu`

## quick start

most arguments are the same as qemu, except you need to change `-` to `--`, for example `-kernel`(in qemu) to `--kernel`(in kemu)

if you have already build a linux kernel bzImage and initramfs, start the hypervisor by running the following cmd 

```bash
./kemu --kernel <bzImage> --disk <disk>
```

## feature


## doc

see more info 

## reference

- [kvmtool](https://github.com/kvmtool/kvmtool)
- [blogspot david942j](https://david942j.blogspot.com/2018/10/note-learning-kvm-implement-your-own.html)