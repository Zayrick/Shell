menu(type='back' mode="single" where=io.dir.exists(sel.path) menu='new' pos="top" title=loc.new_folder image=icon.new_folder)
{
	item(title=loc.datetime cmd=io.dir.create(sys.datetime("ymdHMSs")))
	item(title=loc.guid cmd=io.dir.create(str.guid))
}

menu(type='back' mode="single" where=io.dir.exists(sel.path) menu='new' pos="top" title=loc.new_file image=icon.new_file sep="bottom")
{
	$dt = sys.datetime("ymdHMSs")
	item(title='TXT' cmd=io.file.create('@(dt).txt', 'Hello World!'))
	item(title='XML' cmd=io.file.create('@(dt).xml', '<root>Hello World!</root>'))
	item(title='JSON' cmd=io.file.create('@(dt).json', '[]'))
	item(title='HTML' cmd=io.file.create('@(dt).html', "<html>\n\t<head>\n\t</head>\n\t<body>Hello World!\n\t</body>\n</html>"))
}
