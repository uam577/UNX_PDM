# frozen_string_literal: true

require 'faraday'

# HTTP OBS client for searches
module OBS
  # Error class for unsupported search terms
  class InvalidSearchTerm < StandardError; end

  # One binary that represents an rpm.
  # It has the following properties
  # Binary.arch => [String]
  # Binary.baseproject => [String]
  # Binary.filename => [String]
  # Binary.filepath => [String]
  # Binary.name => [String]
  # Binary.package => [String]
  # Binary.project => [String]
  # Binary.release => [String]
  # Binary.repository => [String]
  # Binary.type => [String]
  # Binary.version => [String]
  # Binary.relevance => [Integer]
  # The following properities may not be available
  # Binary.description => [String]
  # Binary.summary => [String]
  # Binary.size => [Integer]
  # Binary.quality => [String]
  class Binary < Hashie::Mash
    include Hashie::Extensions::Mash::SymbolizeKeys
    def self.coerce(binary)
      binary = Array(binary)
      binary.map { |bin| Binary.new(bin) }
    end
  end

  # A collection of one more binaries with the following properties
  # Collection.binaries => [Array] of [Binary]
  # Collection.matches  => [Integer]
  class Collection < Hashie::Mash
    include Hashie::Extensions::Mash::SymbolizeKeys
    include Hashie::Extensions::Coercion
    coerce_key :binary, Binary
    coerce_key :matches, Integer

    def self.coerce(hash)
      c = Collection.new(hash)
      c[:binaries] = c.delete(:binary)
      c
    end
  end

  # Helper class that turns a "collection" attribute in HTTP responses into an
  # instance of [Collection]
  class Response < Hashie::Mash
    include Hashie::Extensions::Mash::SymbolizeKeys
    include Hashie::Extensions::Coercion
    coerce_key :collection, Collection
  end

  # Utility function to generate a xpath query from a search term and the following options:
  # @param [Hash] opts the options for the query.
  # @option opts [String] :baseproject OBS base project used to search packages for
  # @option opts [String] :project OBS specific project used to search packages into
  # @option opts [Boolean] :exclude_debug Exclude Debug packages from search
  # @option opts [String] :exclude_filter Exclude packages containing this term
  #
  def self.xpath_for(query, opts = {})
    baseproject = opts[:baseproject]
    project = opts[:project]
    exclude_debug = opts[:exclude_debug]
    exclude_filter = opts[:exclude_filter]

    words = query.split.reject { |part| part.match(/^[0-9_.-]+$/) }
    versrel = query.split.select { |part| part.match(/^[0-9_.-]+$/) }
    Rails.logger.debug "splitted words and versrel: #{words.inspect} #{versrel.inspect}"
    raise InvalidSearchTerm, 'Please provide a valid search term' if words.blank? && versrel.blank?
    if words.blank? && versrel.present?
      raise InvalidSearchTerm, 'The package name is required when searching for a version'
    end

    xpath_items = []
    xpath_items << "@project = '#{project}' " unless project.blank?
    substring_words = words.reject { |word| word.match(/^".+"$/) }.map { |word| "'#{word.gsub(/['"()]/, '')}'" }.join(', ')
    xpath_items << "contains-ic(@name, #{substring_words})" unless substring_words.blank?
    words.select { |word| word.match(/^".+"$/) }.map { |word| word.delete('"') }.each do |word|
      xpath_items << "@name = '#{word.gsub(/['"()]/, '')}' "
    end
    xpath_items << "path/project='#{baseproject}'" unless baseproject.blank?
    xpath_items << "not(contains-ic(@project, '#{exclude_filter}'))" if !exclude_filter.blank? && project.blank?
    xpath_items << versrel.map { |part| "starts-with(@versrel,'#{part}')" }.join(' and ') unless versrel.blank?
    if exclude_debug
      xpath_items << "not(contains-ic(@name, '-debuginfo')) and not(contains-ic(@name, '-debugsource')) " \
                     "and not(contains-ic(@name, '-devel')) and not(contains-ic(@name, '-lang'))"
    end
    xpath_items.join(' and ')
  end

  class << self
    attr_writer :client
    attr_reader :configuration
  end

  # Configure client
  #
  # @yield [configuration] configuration object to defining client parameters
  # @example
  #   OBS.configure do |config|
  #     config.api_key = "XYZ"
  #     config.adapter = :typhoeus
  #   end
  def self.configure
    @configuration ||= Configuration.new
    yield(@configuration) if block_given?

    self.client = Faraday.new(@configuration.api_host) do |conn|
      conn.basic_auth @configuration.api_username, @configuration.api_password
      conn.request :url_encoded
      conn.response :logger, Rails.logger, headers: false
      conn.response :mashify, mash_class: Response
      conn.response :raise_error
      conn.use FaradayMiddleware::ParseXml, content_type: /\bxml$/
      conn.adapter @configuration.adapter

      conn.headers['User-Agent'] = 'software.o.o'
      conn.headers['X-Username'] = @configuration.api_username
      conn.headers['X-opensuse_data'] = @configuration.opensuse_cookie if @configuration.opensuse_cookie
    end
  end

  def self.client
    @client || configure
  end

  # HTTP client configuration wrapper
  class Configuration
    attr_accessor :api_host, :api_username, :api_password, :opensuse_cookie, :adapter

    def initialize
      @adapter = Faraday.default_adapter
    end
  end

  # Searches for published binaries
  #
  # Passes these options to xpath_for:
  # @param [String] query term to search for
  # @param [Hash] opts the options for the query.
  # @option opts [String] :baseproject OBS base project used to search packages for
  # @option opts [String] :project OBS specific project used to search packages into
  # @option opts [Boolean] :exclude_debug Exclude Debug packages from search
  # @option opts [String] :exclude_filter Exclude packages containing this term
  #
  def self.search_published_binary(query, opts = {})
    cache_key = ActiveSupport::Cache.expand_cache_key([query, opts])
    Rails.cache.fetch(cache_key, expires_in: 2.hours) do
      result = OBS.client.get('/search/published/binary/id', match: xpath_for(query, opts)).body.collection
      return [] unless result.binaries

      result.binaries.each do |bin|
        bin_with_quality = OBS.add_project_quality(bin)
        OBS.add_binary_relevance(bin_with_quality, query)
      end

      result.binaries.sort_by { |bin| - bin.relevance }
    end
  end

  # Add relevance for sorting of binaries
  # @param [Binary] binary to add relevance to
  # @param [String] search query for comparison of match
  #
  # @return [Binary]
  def self.add_binary_relevance(binary, query)
    quoted_query = Regexp.quote(query)
    binary.relevance = 0
    binary.relevance += 15 if /^#{quoted_query}$/i.match?(binary.name)
    binary.relevance += 5 if /^#{quoted_query}/i.match?(binary.name)
    binary.relevance += 15 if /^openSUSE:/i.match?(binary.project)
    binary.relevance += 5 if /^#{quoted_query}$/i.match?(binary.project)
    binary.relevance += 2 if /^#{quoted_query}/i.match?(binary.project)
    binary.relevance -= 5 if /unstable/i.match?(binary.project)
    binary.relevance -= 10 if /^home:/.match?(binary.project)
    binary.relevance -= 20 if /^openSUSE:Maintenance/i.match?(binary.project)
    binary.relevance -= 10 if /-debugsource$/.match?(binary.name)
    binary.relevance -= 10 if /-debuginfo$/.match?(binary.name)
    binary.relevance -= 3 if /-devel/i.match?(binary.name)
    binary.relevance -= 3 if /-doc$/.match?(binary.name)
    binary
  end

  # Add project quality attribute for filtering
  # @param [Binary] to add its project's quality attribute to
  #
  # @return [Binary]
  def self.add_project_quality(binary)
    begin
      binary.quality = OBS.search_project_quality(binary.project)
    rescue Faraday::ClientError => e
      # Attribute search might fail, not all projects have accessible info
      raise unless e.response[:status] == 404

      # Assume the project is NOT private if search fails
      binary.quality = ''
    end
    binary
  end

  # Add description, summary and size to binary
  # @param [Binary] to add fileinfo to
  #
  # @return [Binary]
  def self.add_fileinfo_to_binary(binary)
    begin
      fileinfo = OBS.search_published_binary_fileinfo(binary)
    rescue Faraday::ClientError => e
      # Not every binary has published fileinfo. Skip if that is the case
      raise unless e.response[:status] == 404
    else
      binary.description = fileinfo.description if fileinfo.description.present?
      binary.summary = fileinfo.summary if fileinfo.summary.present?
      # .size is a built-in method and has to be accessed via #[]
      binary[:size] = fileinfo[:size] if fileinfo[:size].present?
    end
    binary
  end

  # Searches for published fileinfo of a binary
  # @param [Binary] binary to search fileinfo for
  #
  # @return [Fileinfo]
  def self.search_published_binary_fileinfo(binary)
    cache_key = ActiveSupport::Cache.expand_cache_key(binary, 'fileinfo')
    Rails.cache.fetch(cache_key, expires_in: 2.hours) do
      url = "/published/#{binary.project}/#{binary.repository}/#{binary.arch}/#{binary.filename}?view=fileinfo"
      OBS.client.get(url).body.fileinfo
    end
  end

  # Searches for the OBS:QualityCategory attribute of a given project
  # @param [String] project OBS specific project to search attribute for
  #
  # @return [String]
  def self.search_project_quality(project)
    cache_key = ActiveSupport::Cache.expand_cache_key(project, 'attribute')
    Rails.cache.fetch(cache_key, expires_in: 2.hours) do
      xml_quality = OBS.client.get("/source/#{project}/_attribute/OBS:QualityCategory")

      xml_quality && xml_quality[:attribute] ? xml_quality[:attribute][:text].strip : ''
    end
  end
end
