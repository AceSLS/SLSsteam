#!/bin/bash

SLSDIR="$HOME/.local/share/SLSsteam"
SLSPATH="$SLSDIR/path"
SLSLIB="$SLSDIR/SLSsteam.so"
SLSAUDIT="LD_AUDIT=\"$SLSDIR/library-inject.so:$SLSDIR/SLSsteam.so\""

uninstall()
{
	test -f "$SLSDIR/steam-jupiter.bak" && sudo cp -v "$SLSDIR/steam-jupiter.bak" "$(realpath "$(type -P steam-jupiter)")" #Left over from Steam Deck patcher
	rm -v "$HOME/.config/fish/SLSsteam.fish" 2> /dev/null
	rm -v "$HOME/.local/share/applications/steam.desktop" 2> /dev/null
	rm -v "$HOME/.local/share/applications/steam-native.desktop" 2> /dev/null
	rm -rvf "$SLSDIR"
	echo "Uninstall done!"
}

install_wrapper()
{
	EXE="$1"
	FPATH="$(type -P $EXE)"

	DIRNAME="$(dirname "$FPATH")"
	if [ "$DIRNAME" = "$SLSPATH" ]; then
		echo "$EXE wrapper already installed! Skipping"
		return 0
	fi

	if [[ $? -ne 0 ]]; then
		echo "$EXE not found in path! Skipping"
		return 1
	fi

	echo -e "#!/bin/sh\n$SLSAUDIT \"$FPATH\"" > "$SLSPATH/$EXE"

	chmod u+x "$SLSPATH/$EXE"

	echo "Created wrapper for $FPATH at $SLSPATH/$EXE"
	return 0
}

install_desktop_file()
{
	NAME="$1.desktop"
	USR_APP_DIR="$HOME/.local/share/applications"
	APP_DIR="/usr/share/applications"

	#All these error checks are borderline insane, but I won't assume anything anymore.
	if [ ! -f "$APP_DIR/$NAME" ]; then
		echo "$NAME not found in applications! Skipping"
		return 1
	fi

	if [ ! -d "$USR_APP_DIR" ]; then
		mkdir "$$USR_APP_DIR"
		if [[ $? -ne 0 ]]; then
			echo "Failed to create $USR_APP_DIR! Aborting .desktop creation"
			return 1
		fi
	fi

	cp "$APP_DIR/$NAME" "$USR_APP_DIR/"
	sed -i "s|^Exec=/|Exec=env $SLSAUDIT /|" "$USR_APP_DIR/$NAME"

	echo "Created $USR_APP_DIR/$NAME"
}

install_path() {
    # Detect the current shell
    CURRENT_SHELL=$(ps -p $$ -o comm= | tr -d '[:space:]')

    if [ "$CURRENT_SHELL" = "fish" ]; then
        SLSSTEAM_FISH="$HOME/.config/fish/conf.d/SLSsteam.fish"
        mkdir -p "$(dirname "$SLSSTEAM_FISH")"

        # Fish command to update PATH
        CMD="set -gx PATH $SLSPATH \$PATH"

        if [ ! -f "$SLSSTEAM_FISH" ]; then
            echo "$CMD" > "$SLSSTEAM_FISH"
            echo "Wrote $CMD to $SLSSTEAM_FISH"
            echo "Relog for changes to take effect!"
        else
            echo "PATH already configured for fish"
        fi
        return 0
    else
        # If fish exists on the system but is not the current shell, still install config
        if command -v fish >/dev/null 2>&1; then
            SLSSTEAM_FISH="$HOME/.config/fish/conf.d/SLSsteam.fish"
            mkdir -p "$(dirname "$SLSSTEAM_FISH")"
            CMD="set -gx PATH $SLSPATH \$PATH"

            if [ ! -f "$SLSSTEAM_FISH" ]; then
                echo "$CMD" > "$SLSSTEAM_FISH"
                echo "Wrote $CMD to $SLSSTEAM_FISH"
                echo "You can start a fish shell later to pick up the PATH changes."
            else
                echo "PATH already configured for fish"
            fi
            return 0
        fi

        # Fish not found on the system, skip installation
        echo "Fish not detected. Skipping path installation."
        return 1
    fi
}

install_slssteam()
{
	LIB="./bin/SLSsteam.so"
	if [ ! -f "$LIB" ]; then
		echo "bin/SLSsteam.so not found! Did you run the install.sh in the correct directory?"
		exit 1
	fi

	if [ ! -d "$SLSDIR" ]; then
		#Not using -p flag because it will silence errors
		#Although I don't think there's anyone that doesn't have a ~/.local/share directory
		mkdir "$SLSDIR"
		if [[ $? -ne 0 ]]; then
			echo "Unable to create $SLSDIR! Aborting"
			exit 1
		fi
	fi

	if [ ! -d "$SLSPATH" ]; then #This whole fucking block should be unnecessary. Well, better safe than sorry. Thanks Valve for the Deck
		mkdir "$SLSPATH"

		if [[ $? -ne 0 ]]; then
			echo "Unable to create $SLSPATH! Aborting"
			exit 1
		fi
	fi

	cp -v ./bin/* "$SLSDIR/"
}

install_all()
{
	install_slssteam

	install_path
	if [[ $? -eq 0 ]]; then
		install_wrapper steam
		install_wrapper steam-runtime
		#Wrapping the steam-jupiter doesn't work, probably doesn't get called from PATH
		install_wrapper steam-native
	fi

	install_desktop_file steam
	#No steam-runtime.desktop (atleast on my Arch install...)
	install_desktop_file steam-native

	echo "Install script done! If any wrappers or .desktop files have been created it was successfull."
}

if [[ $# -lt 1 ]]; then
	echo "Usage: $0 install|uninstall"
	exit 0
fi

if [ "$1" == "install" ]; then
	install_all
elif [ "$1" == "uninstall" ]; then
	uninstall
else
	echo "Unknown command $1!"
	exit 1
fi
