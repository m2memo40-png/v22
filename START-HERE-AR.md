# ابدأ من هنا — Fluxmax v23

هذه الحزمة مصدر مشروع **Fluxmax** الذي يستخدم نواته المستقلة. اسم ISO المتوقع هو `build/Fluxmax-x86_64.iso` وموجه الصدفة هو `fluxmax>`.

## رفع النسخة إلى مستودع GitHub الحالي

1. نزّل ملف ZIP واستخرجه على الكمبيوتر.
2. افتح مستودع Fluxmax الحالي على جهازك. انسخ **محتويات** المجلد المستخرج إلى جذر المستودع، مع الاحتفاظ بمجلد `.git` الموجود وعدم وضع ملف ZIP داخل المستودع.
3. راجع التغييرات ثم نفّذ commit وpush إلى فرع `main` أو `master`.
4. افتح GitHub → Actions وشغّل `Build Fluxmax ISO and publish Release`.
5. لا تعتبر الإصدار ناجحًا حتى ينجح البناء ومرحلتَا QEMU لكل من RTL8139 وE1000. عند فشل أي خطوة، نزّل/انسخ سجلها كاملًا، وبالأخص السطور الأخيرة من الاختبار الذي فشل.

## البناء والاختبار محليًا على Linux

```sh
make clean
make -j2 check user-elf test
bash scripts/check-branding.sh
bash scripts/check-limine-config.sh limine.conf
```

لبناء ISO يلزم إصدار Limine الثنائي المطابق وأداة `xorriso`:

```sh
make iso LIMINE_DIR=/path/to/extracted/limine-binary
```

ولتجربة QEMU يلزم تثبيت `qemu-system-x86_64` وتشغيل اختبارات السكربت كما هو موضح في README.

## ملاحظة مهمة عن GNOME

السطح الحالي هو **Fluxmax Desktop المخصص** فوق النواة الخاصة، وليس GNOME الأصلي. لا يوجد بعد توافق POSIX أو Wayland/Mutter أو خدمات D-Bus ومكتبات GTK اللازمة لتشغيل GNOME. لا تحاول استبدال النواة أو اعتبار الواجهة GNOME مكتملة؛ اتبع `GNOME-PORT-ROADMAP_AR.md` بعد اجتياز اختبارات الإقلاع.
