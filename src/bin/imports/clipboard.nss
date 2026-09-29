item(type='back' where=io.dir.exists(sel.path) and clipboard.can_paste_link
	title=loc.paste_symbolic_link image=icon.paste_shortcut
	tip=loc.paste_symbolic_link_tip pos=indexof(str.replace(title.paste_shortcut, '&', ''), 1)
	cmd=command.paste_symlink(sel.path))

menu(where=sel.count>0 type='file|dir|drive|namespace|back' mode="multiple" title=title.copy_path image=icon.copy_path)
{
	item(where=sel.count > 1 title=loc.copy_multiple_paths cmd=command.copy(sel(false, "\n")))
	item(mode="single" title=@sel.path tip=sel.path cmd=command.copy(sel.path))
	item(mode="single" type='file' separator="before" where=length(sel.lnk)>0 title=sel.lnk cmd=command.copy(sel.lnk))
	separator
	item(mode="single" where=@sel.parent.len>3 title=sel.parent cmd=@command.copy(sel.parent))
	separator
	item(mode="single" type='file|dir|back.dir' title=sel.file.name cmd=command.copy(sel.file.name))
	item(mode="single" type='file' where=sel.file.len != sel.file.title.len title=@sel.file.title cmd=command.copy(sel.file.title))
	item(mode="single" type='file' where=sel.file.ext.len>0 title=sel.file.ext cmd=command.copy(sel.file.ext))
}
