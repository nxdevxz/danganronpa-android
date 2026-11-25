# Danganronpa Android

An experimental Android port of the Nintendo Switch version of **Danganronpa: Trigger Happy Havoc Anniversary Edition**. Version **0.0.1** introduces the launcher, app icon and data folder. Its Android package, `io.github.nxdevxz.danganronpa`, allows it to be installed alongside P4G Android.

Game boot, importing game files, save compatibility and shader support still need adaptation and testing.

## Building the APK

Run the [Danganronpa Android workflow](.github/workflows/build-danganronpa.yml) to build `Danganronpa.apk`. The workflow uses the repository secrets `DANGANRONPA_SIGNING_KEY_B64` and `DANGANRONPA_SIGNING_PASSWORD` for signing. After a successful build, download the `Danganronpa-0.0.1` artifact. [Source checks](.github/workflows/check-danganronpa.yml) run on every push and pull request.

This project is based on [P4G Android](https://github.com/nxdevxz/p4g-android). See the [license](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md) for credits and licensing details.
