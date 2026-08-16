set tags=out/tags

cscope kill out
cscope add out/cscope.out

let &grepprg="grep -n $([ -f .greprc ] && grep -v '^\\#' .greprc) $* /dev/null"
