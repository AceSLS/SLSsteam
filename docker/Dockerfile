FROM debian:bookworm

#Install dependencies
RUN apt-get -y update && apt-get -y install g++-multilib libssl-dev libcurl4-openssl-dev libluajit-5.1-dev pkg-config make wget p7zip-full mold git libdbus-1-dev

#Install dotnet-9.0
RUN wget "https://packages.microsoft.com/config/debian/12/packages-microsoft-prod.deb" -O packages-microsoft-prod.deb && dpkg -i packages-microsoft-prod.deb && rm packages-microsoft-prod.deb && apt-get -y update && apt-get install -y dotnet-sdk-9.0

ENV PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig"

RUN useradd -m sls
USER sls
