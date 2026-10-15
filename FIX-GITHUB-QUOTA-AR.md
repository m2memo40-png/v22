# إصلاح فشل رفع تشخيصات GitHub Actions

ظهرت رسالة `Artifact storage quota has been hit`. هذه مشكلة تخزين في GitHub Actions وليست علة في النواة. في v12 أزيل رفع Artifact الخاص بالتشخيصات ويطبع QEMU driver معاينة شاشة VGA ASCII إلى سجل المهمة. يمكن حذف Artifacts قديمة من تبويب Actions أو انتظار إعادة احتساب الحصة، لكن البناء لم يعد يعتمد على نجاح رفعها.
