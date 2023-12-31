## Config.JSON

When the XData server first starts, it checks to see if a configuration JSON file is available. XData first checks the application directory (wherever the application is launched from) for a .json file with the same name as the application (eg: project1.json). This location can be overriden by passsing a CONFIG parameter (eg: CONFIG=c:\data\config.json) to the XData application. If neither are available, defaults will provided for all values.  As a result, the configuration file is entirely optional. This is the expected configuration during intial development and testing.  However, once the project is deployed, a configuration JSON file will most likely be needed. This is how the BaseURL property is set, if it isn't otherwise altered in code.

As this project unfolds, it is likely that additional elements will be added here. 

### BaseURL
This is used to set the BaseURL for the XData server.  For example, in a production environment, this might be something like "https://+9999/tms/xdata" while in a development environment it might be "http://+2001/tms/xdata".  Production servers, particularly those that are public-facing, should be configured with an HTTPS protocol, with the necessary SSL certificates.  Note that regardless of the environment, the TMS HTTP Config Tool, or equivalent, should be used to reserve the port number on the system that XData is running on. This includes adding an SSL certificate if the HTTPS protocol is used.

### Cache Folder
Endpoints that return images, specifically the ChatService/GetChatImage endpoint, will generate a cache of any images requested from the database.  This may include both thumbnails as well as the original image stored in the database.  By default, a cache folder will be created in the same folder as the XData application when it first starts.  This behaviour can be overriden by setting a "Cache Folder" entry in the configuration. Eg: c:/data/cache.  Note carefully that the folders are specified using a forward slash.

### Chat Interface
For the chat features of this project to work properly, appropriate API keys need to be provided in most cases (eg: OpenAI's ChatGPT offernings). The configuration JSON file is used to provide these keys, along with several other chat-related parameters, to the XData application. As there may very well be several chat interfaces provided, a JSON array is used in this case. An example is provided below. Please refer to the [ChatService documentation](https://github.com/500Foods/TMS-XData-TemplateDemoData/blob/main/docs/ChatService.md) for more detailed information.

### Example 

Here is an example JSON configuration file.

```
{
  "BaseURL": "http://+:12345/tms/xdata",
  "Cache Folder": "C:/Data/Cache",
  "Chat Interface": [
    {
      "Name": "ChatGPT 3.5",
      "Default": true,
      "Model": "gpt-3.5-turbo",
      "Organization": "your-org-identifier-here",
      "API Key": "your-api-key-here",
      "Endpoint": "https://api.openai.com/v1/chat/completions",
      "Limit": 4096,
      "Cost Prompt": 0.000002,
      "Cost Completion": 0.000002
    },
    {
      "Name": "ChatGPT 4 8K",
      "Default": false,
      "Model": "gpt-4-8k",
      "Organization": "your-org-identifier-here",
      "API Key": "your-api-key-here",
      "Endpoint": "https://api.openai.com/v1/chat/completions",
      "Limit": 8192,
      "Cost Prompt": 0.00003,
      "Cost Completion": 0.00006
    },
    {
      "Name": "ChatGPT 4 32K",
      "Default": false,
      "Model": "gpt-4-32k",
      "Organization": "your-org-identifier-here",
      "API Key": "your-api-key-here",
      "Endpoint": "https://api.openai.com/v1/chat/completions",
      "Limit": 32768,
      "Cost Prompt": 0.00006,
      "Cost Completion": 0.00012
    },
    {
      "Name": "Image 256",
      "Default": false,
      "Model": "dall-e 256",
      "Organization": "your-org-identifier-here",
      "API Key": "your-api-key-here",
      "Endpoint": "https://api.openai.com/v1/images/generations",
      "Limit": 1000,
      "Cost": 0.016
    },
    {
      "Name": "Image 512",
      "Default": false,
      "Model": "dall-e 512",
      "Organization": "your-org-identifier-here",
      "API Key": "your-api-key-here",
      "Endpoint": "https://api.openai.com/v1/images/generations",
      "Limit": 1000,
      "Cost": 0.018
    },
    {
      "Name": "Image 1024",
      "Default": false,
      "Model": "dall-e 1024",
      "Organization": "your-org-identifier-here",
      "API Key": "your-api-key-here",
      "Endpoint": "https://api.openai.com/v1/images/generations",
      "Limit": 1000,
      "Cost": 0.02
    }
  ]
}
```
