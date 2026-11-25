use std::ptr;
#[repr(C)] #[derive(Clone,Copy)] struct Message {bytes:[u8;64],length:u16}
#[repr(C)] struct Mailbox {slots:[Message;8],head:u8,tail:u8,count:u8}
unsafe extern "C" {fn ores_embedded_init(box_ptr:*mut Mailbox)->i32;fn ores_embedded_push(box_ptr:*mut Mailbox,data:*const u8,length:usize)->i32;fn ores_embedded_remaining_capacity(box_ptr:*const Mailbox,available:*mut usize)->i32;}
#[test] fn capacity_exhaustion_and_corruption(){let mut box_ptr=std::mem::MaybeUninit::<Mailbox>::zeroed();assert_eq!(unsafe{ores_embedded_init(box_ptr.as_mut_ptr())},0);let mut mb=unsafe{box_ptr.assume_init()};let mut cap=99usize;assert_eq!(unsafe{ores_embedded_remaining_capacity(&mb,&mut cap)},0);assert_eq!(cap,8);let data=[42u8];for n in 0..8{assert_eq!(unsafe{ores_embedded_push(&mut mb,data.as_ptr(),1)},0);assert_eq!(unsafe{ores_embedded_remaining_capacity(&mb,&mut cap)},0);assert_eq!(cap,7-n);}assert_eq!(unsafe{ores_embedded_push(&mut mb,data.as_ptr(),1)},3);cap=99;assert_eq!(unsafe{ores_embedded_remaining_capacity(&mb,ptr::null_mut())},1);let alias=(&mut mb as *mut Mailbox).cast::<usize>();assert_eq!(unsafe{ores_embedded_remaining_capacity(&mb,alias)},1);mb.tail=255;assert_eq!(unsafe{ores_embedded_remaining_capacity(&mb,&mut cap)},1);assert_eq!(cap,0);}

unsafe extern "C" {
    fn ores_embedded_pop(box_ptr: *mut Mailbox, destination: *mut u8,
        capacity: usize, written: *mut usize) -> i32;
}
#[test]
fn bounded_capacity_tracks_wraparound_and_failed_pop() {
    let mut raw = std::mem::MaybeUninit::<Mailbox>::zeroed();
    assert_eq!(unsafe { ores_embedded_init(raw.as_mut_ptr()) }, 0);
    let mut mb = unsafe { raw.assume_init() };
    for round in 0..16u8 {
        let data = [round, round.wrapping_add(1)];
        let mut available = 99usize;
        for count in 0..8usize {
            assert_eq!(unsafe { ores_embedded_remaining_capacity(&mb, &mut available) }, 0);
            assert_eq!(available, 8-count);
            assert_eq!(unsafe { ores_embedded_push(&mut mb, data.as_ptr(), data.len()) }, 0);
        }
        assert_eq!(unsafe { ores_embedded_remaining_capacity(&mb, &mut available) }, 0);
        assert_eq!(available, 0);
        let mut written = 99usize;
        let mut output = [0xa5u8; 2];
        assert_eq!(unsafe { ores_embedded_pop(&mut mb, output.as_mut_ptr(), 1, &mut written) }, 5);
        assert_eq!(written, 0);
        assert_eq!(output, [0xa5u8; 2]);
        assert_eq!(unsafe { ores_embedded_remaining_capacity(&mb, &mut available) }, 0);
        assert_eq!(available, 0);
        for count in 1..=8usize {
            assert_eq!(unsafe { ores_embedded_pop(&mut mb, output.as_mut_ptr(), output.len(), &mut written) }, 0);
            assert_eq!(written, 2);
            assert_eq!(output, data);
            assert_eq!(unsafe { ores_embedded_remaining_capacity(&mb, &mut available) }, 0);
            assert_eq!(available, count);
        }
        written = 99;
        assert_eq!(unsafe { ores_embedded_pop(&mut mb, output.as_mut_ptr(), output.len(), &mut written) }, 4);
        assert_eq!(written, 0);
    }
}
