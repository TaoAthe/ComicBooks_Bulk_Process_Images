// ebayscrape.js
// eBay scraping utility for comic book pricing and market data using SerpAPI

const axios = require('axios');

// SerpAPI configuration
const SERPAPI_KEY = '0d17888ebb6912c39b6abb5c18b3cc183691deee6fb444bb7aa5e9fd22772a7c';
const SERPAPI_BASE_URL = 'https://serpapi.com/search.json';

// Basic eBay scraper class using SerpAPI
class EbayScraper {
    constructor() {
        this.apiKey = SERPAPI_KEY;
    }

    /**
     * Search for comic books on eBay using SerpAPI
     * @param {string} query - Search query (e.g., "Batman #1")
     * @param {number} limit - Maximum number of results
     * @returns {Promise<Array>} Array of listing objects with title, issue, price, etc.
     */
    async searchComics(query, limit = 10) {
        try {
            const params = {
                engine: 'ebay',
                q: query,
                api_key: this.apiKey,
                _nkw: query, // eBay specific parameter
                LH_ItemCondition: '3000', // Used condition for comics
                _sacat: '0' // All categories
            };

            const response = await axios.get(SERPAPI_BASE_URL, { params });

            if (!response.data.organic_results) {
                console.log('No results found for query:', query);
                return [];
            }

            const results = response.data.organic_results
                .slice(0, limit)
                .map(item => {
                    const parsed = this.parseComicTitle(item.title);
                    return {
                        title: parsed.title,
                        issue: parsed.issue,
                        full_title: item.title,
                        price: this.parsePrice(item.price),
                        link: item.link,
                        image: item.thumbnail,
                        condition: item.condition || 'Used',
                        timestamp: new Date().toISOString(),
                        source: 'ebay'
                    };
                })
                .filter(item => item.title && item.price > 0);

            return results;
        } catch (error) {
            console.error('Error with SerpAPI:', error.message);
            return [];
        }
    }

    /**
     * Parse comic book title to extract title and issue number
     * @param {string} fullTitle - Full eBay listing title
     * @returns {Object} Object with title and issue
     */
    parseComicTitle(fullTitle) {
        if (!fullTitle) return { title: '', issue: '' };

        // Common patterns: "Batman #1", "Spider-Man Vol. 1 #1", "X-Men #25 (1990)"
        const patterns = [
            /(.+?)\s*#(\d+)(?:\s*\([^)]*\))?/i,  // "Title #Issue"
            /(.+?)\s*Vol\.?\s*(\d+)\s*#(\d+)/i,   // "Title Vol X #Issue"
            /(.+?)\s*#(\d+[a-z]?)/i               // "Title #Issue" with letters
        ];

        for (const pattern of patterns) {
            const match = fullTitle.match(pattern);
            if (match) {
                let title = match[1].trim();
                let issue = match[2];

                // Clean up title
                title = title.replace(/\s+/g, ' ').trim();
                // Remove common suffixes
                title = title.replace(/\s+(comic|book|series|collection)$/i, '');

                return { title, issue };
            }
        }

        // Fallback: return full title as title, no issue
        return { title: fullTitle.trim(), issue: '' };
    }

    /**
     * Parse price string to numeric value
     * @param {string|Object} price - Price from SerpAPI
     * @returns {number} Numeric price
     */
    parsePrice(price) {
        if (typeof price === 'string') {
            const match = price.match(/\$([0-9,]+\.?[0-9]*)/);
            return match ? parseFloat(match[1].replace(',', '')) : 0;
        } else if (typeof price === 'object' && price.raw) {
            return parseFloat(price.raw.replace(/[$,]/g, '')) || 0;
        }
        return 0;
    }

    /**
     * Get average price for a comic book
     * @param {string} query - Comic book title/issue
     * @returns {Promise<Object>} Price statistics
     */
    async getAveragePrice(query) {
        const listings = await this.searchComics(query, 20);

        if (listings.length === 0) {
            return { average: 0, min: 0, max: 0, count: 0, title: '', issue: '' };
        }

        const prices = listings.map(listing => listing.price).filter(price => price > 0);

        if (prices.length === 0) {
            return { average: 0, min: 0, max: 0, count: 0, title: '', issue: '' };
        }

        const average = prices.reduce((sum, price) => sum + price, 0) / prices.length;
        const min = Math.min(...prices);
        const max = Math.max(...prices);

        // Get most common title/issue from results
        const titleCount = {};
        const issueCount = {};

        listings.forEach(listing => {
            if (listing.title) {
                titleCount[listing.title] = (titleCount[listing.title] || 0) + 1;
            }
            if (listing.issue) {
                issueCount[listing.issue] = (issueCount[listing.issue] || 0) + 1;
            }
        });

        const mostCommonTitle = Object.keys(titleCount).reduce((a, b) => titleCount[a] > titleCount[b] ? a : b, '');
        const mostCommonIssue = Object.keys(issueCount).reduce((a, b) => issueCount[a] > issueCount[b] ? a : b, '');

        return {
            average: Math.round(average * 100) / 100,
            min,
            max,
            count: prices.length,
            title: mostCommonTitle,
            issue: mostCommonIssue,
            listings: listings // Include full listings for historical storage
        };
    }

    /**
     * Store pricing data for historical tracking
     * @param {string} query - Comic query
     * @param {Object} data - Price data to store
     */
    async storeHistoricalData(query, data) {
        // In a real implementation, this would save to a database
        // For now, just log it
        const historicalEntry = {
            query,
            timestamp: new Date().toISOString(),
            data
        };

        console.log('Historical data entry:', JSON.stringify(historicalEntry, null, 2));

        // TODO: Implement database storage (MongoDB, SQLite, etc.)
        // This could be used to track price changes over time
    }
}

// Export for use in other modules
module.exports = EbayScraper;