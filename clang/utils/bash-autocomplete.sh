# Please add "source /path/to/bash-autocomplete.sh" to your .bashrc to use this.

_clang() {
  local cur prev words cword arg
  _init_completion -n : || return

  local flags=$( clang --list-options )

  # bash always separates '=' as a token even if there's no space before/after '='.
  # On the other hand, '=' is just a regular character for clang options that
  # contain '='. For example, "-stdlib=" is defined as is, instead of "-stdlib" and "=".
  # So, we need to partially undo bash tokenization here for integrity.
  local w1="${COMP_WORDS[$cword - 1]}"
  local w2="${COMP_WORDS[$cword - 2]}"
  if [[ "$cur" == -* ]]; then
    # -foo<tab>
    COMPREPLY=( $(COMP_WORDBREAKS= compgen -W "$flags" -- "$cur") )
  elif [[ "$w1" == -*  && "$cur" == '=' ]]; then
    # -foo=<tab>
    for x in $(COMP_WORDBREAKS= compgen -W "$flags" -- "$w1="); do
      COMPREPLY+=(${x#*=})
    done
  elif [[ "$w1" == -* ]]; then
    # -foo <tab> or -foo bar<tab>
    flags=$( clang --autocomplete="$w1,$cur" )
    COMPREPLY=( $( compgen -W "$flags" -- "") )
  elif [[ "$w2" == -* && "$w1" == '=' ]]; then
    # -foo=bar<tab>
    for x in $(COMP_WORDBREAKS= compgen -W "$flags" -- "$w2=$cur"); do
      COMPREPLY+=(${x#*=})
    done
  else
    _filedir
  fi
}
complete -F _clang clang
