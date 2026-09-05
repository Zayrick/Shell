menu(mode="multiple" title=loc.develop sep=sep.bottom image=\uE26E)
{
	menu(mode="single" title=loc.editors image=\uE17A)
	{
		item(title='Visual Studio Code' image=[\uE272, #22A7F2] cmd='code' args='"@sel.path"')
		separator
		item(type='file' mode="single" title=loc.windows_notepad image cmd='@sys.bin\notepad.exe' args='"@sel.path"')
	}

	menu(mode="multiple" title='dotnet' image=\uE143)
	{
		item(title=loc.run cmd-line='/K dotnet run' image=\uE149)
		item(title=loc.watch cmd-line='/K dotnet watch')
		item(title=loc.clean image=\uE0CE cmd-line='/K dotnet clean')
		separator
		item(title=loc.build_debug cmd-line='/K dotnet build')
		item(title=loc.build_release cmd-line='/K dotnet build -c release /p:DebugType=None')

		menu(mode="multiple" sep="both" title=loc.publish image=\ue11f)
		{
			$publish='dotnet publish -r win-x64 -c release --output publish /*/p:CopyOutputSymbolsToPublishDirectory=false*/'
			item(title=loc.publish_single_file sep="after" cmd-line='/K @publish --no-self-contained /p:PublishSingleFile=true')
			item(title=loc.framework_dependent_deployment cmd-line='/K @publish')
			item(title=loc.framework_dependent_executable cmd-line='/K @publish --self-contained false')
			item(title=loc.self_contained_deployment cmd-line='/K @publish --self-contained true')
			item(title=loc.single_file cmd-line='/K @publish /p:PublishSingleFile=true /p:PublishTrimmed=false')
			item(title=loc.single_file_trimmed cmd-line='/K @publish /p:PublishSingleFile=true /p:PublishTrimmed=true')
		}
		
		item(title=loc.ef_migrations_add cmd-line='/K dotnet ef migrations add InitialCreate')
		item(title=loc.ef_database_update cmd-line='/K dotnet ef database update')
		separator
		item(title=loc.help image=\uE136 cmd-line='/k dotnet -h')
		item(title=loc.version cmd-line='/k dotnet --info')
	}

	item(type="file" title="reshack" sep="top" image cmd='D:\config\Programs\dev\petools\resource\reshack\reshack.exe' args=sel(1))
	item(type="file" title="peview" image cmd='D:\config\Programs\dev\petools\peview.exe' args=sel(1))
	
	item(type="dir|dir.back" title=loc.web_server vis=key.shift() cmd='D:\config\Programs\dev\web\HTTPServer\WebServer.exe' args='-open -path:"@sel.path"')
}