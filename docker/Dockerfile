FROM debian:bookworm

#Install dependencies
RUN dpkg --add-architecture i386 && apt-get -y update && apt-get -y install g++-multilib libssl-dev:i386 libcurl4-openssl-dev:i386 libluajit-5.1-dev:i386 pkg-config make wget p7zip-full mold git

#Install dotnet-9.0
RUN wget "https://packages.microsoft.com/config/debian/12/packages-microsoft-prod.deb" -O packages-microsoft-prod.deb && dpkg -i packages-microsoft-prod.deb && rm packages-microsoft-prod.deb && apt-get -y update && apt-get install -y dotnet-sdk-9.0

ENV PKG_CONFIG_PATH="/usr/lib/i386-linux-gnu/pkgconfig"

RUN useradd -m sls
USER sls
