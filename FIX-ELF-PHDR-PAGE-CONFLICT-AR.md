# تشخيص وإصلاح Limine: مقاطع ELF ذات صلاحيات مختلفة في الصفحة نفسها

## الدليل من سجل v14

وصل BIOS إلى Limine، وقرأ الإعداد، واختار `boot/kernel.elf`. ثم توقف برسالة:

`PANIC: elf: Attempted to load ELF file with PHDRs with different permissions sharing the same memory page.`

كان ELF القديم يحتوي على مقطع قراءة فقط يبدأ عند `0xffffffff80014000` وينتهي عند `0xffffffff8011c030`، ومقطع `.limine_requests` للقراءة والكتابة يبدأ عند `0xffffffff8011c030`. كلاهما لمس صفحة 4 KiB عند `0xffffffff8011c000`، فرفض Limine تحميل النواة قبل `_start`.

## التعديل

1. تغيّر موضع `.limine_requests` من `ALIGN(8)` إلى `ALIGN(4K)` في `linker.ld`.
2. يفحص `scripts/check-kernel-elf.py` الآن نطاقات الذاكرة المقربة إلى حدود صفحات 4 KiB ويتوقف بخطأ واضح إذا تداخل مقطعان `PT_LOAD` بصلاحيات مختلفة على صفحة واحدة.
3. فحص ELF القديم يكشف التعارض، بينما ELF بعد إعادة الربط يجتاز الفحص ويضع `.limine_requests` في صفحة منفصلة.

## الاختبار المتبقي

اجتياز فحص ELF ليس دليلًا على نجاح الإقلاع الكامل. شغّل GitHub Actions على مصدر v15. المطلوب أن تختفي رسالة `PHDRs with different permissions`، ثم يظهر `FLUXMAX:ENTRY` وبعدها `FLUXMAX:SHELL_READY` / `fluxmax>`. إذا ظهر خطأ جديد فسيكون مرحلة لاحقة نحقق فيها من السجل الجديد.
